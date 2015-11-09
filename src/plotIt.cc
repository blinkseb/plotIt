#include "plotIt.h"
#include <samadhi/Database.h>

// For fnmatch()
#include <fnmatch.h>

#include <TList.h>
#include <TCollection.h>
#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TKey.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLegendEntry.h>
#include <TPaveText.h>
#include <TColor.h>
#include <TGaxis.h>

#include <vector>
#include <map>
#include <fstream>

#include "tclap/CmdLine.h"

#include <boost/regex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <plotters.h>
#include <utilities.h>

namespace fs = boost::filesystem;
using std::setw;

// Load libdpm at startup, on order to be sure that rfio files are working
#include <dlfcn.h>
struct Dummy
{
  Dummy()
  {
    dlopen("libdpm.so", RTLD_NOW|RTLD_GLOBAL);
  }
};
static Dummy foo;

namespace plotIt {

  plotIt::plotIt(const fs::path& outputPath, const std::string& configFile):
    m_outputPath(outputPath) {

      createPlotters(*this);

      gErrorIgnoreLevel = kError;
      m_style.reset(createStyle());
      parseConfigurationFile(configFile);
    }

  int16_t plotIt::loadColor(const YAML::Node& node) {
    std::string value = node.as<std::string>();
    if (value.length() > 1 && value[0] == '#' && ((value.length() == 7) || (value.length() == 9))) {
      // RGB Color
      std::string c = value.substr(1);
      // Convert to int with hexadecimal base
      uint32_t color = 0;
      std::stringstream ss;
      ss << std::hex << c;
      ss >> color;

      float a = 1;
      if (color > 0xffffff) {
        a = (color >> 24) / 255.0;
      }

      float r = ((color >> 16) & 0xff) / 255.0;
      float g = ((color >> 8) & 0xff) / 255.0;
      float b = ((color) & 0xff) / 255.0;

      // Create new color
      m_temporaryObjectsRuntime.push_back(std::make_shared<TColor>(m_colorIndex++, r, g, b, "", a));

      return m_colorIndex - 1;
    } else {
      return node.as<int16_t>();
    }
  }

  void plotIt::parseIncludes(YAML::Node& node) {

    if (node["include"]) {
        std::vector<std::string> files = node["include"].as<std::vector<std::string>>();
        node.remove("include");

        for (std::string& file: files) {
          YAML::Node root = YAML::LoadFile(file);

          for (YAML::const_iterator it = root.begin(); it != root.end(); ++it) {
            node[it->first.as<std::string>()] = it->second;
          }
        }

        if (node["include"])
            parseIncludes(node);
    }

    for(YAML::iterator it = node.begin(); it != node.end(); ++it) {
      if (it->second.Type() == YAML::NodeType::Map)
          parseIncludes(it->second);
    }
  }

  void plotIt::parseConfigurationFile(const std::string& file) {
    YAML::Node f = YAML::LoadFile(file);

    parseIncludes(f);

    if (! f["files"]) {
      throw YAML::ParserException(YAML::Mark::null_mark(), "Your configuration file must have a 'files' list");
    }

    const auto& parseLabelsNode = [](YAML::Node& node) -> std::vector<Label> {
      std::vector<Label> labels;

      for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
        const YAML::Node& labelNode = *it;

        Label label;
        label.text = labelNode["text"].as<std::string>();
        label.position = labelNode["position"].as<Point>();

        if (labelNode["size"])
          label.size = labelNode["size"].as<uint32_t>();

        labels.push_back(label);
      }

      return labels;
    };

    if (f["configuration"]) {
      YAML::Node node = f["configuration"];

      if (node["width"])
        m_config.width = node["width"].as<float>();

      if (node["height"])
        m_config.height = node["height"].as<float>();

      if (node["experiment"])
        m_config.experiment = node["experiment"].as<std::string>();

      if (node["extra-label"])
        m_config.extra_label = node["extra-label"].as<std::string>();

      if (node["luminosity-label"])
        m_config.lumi_label = node["luminosity-label"].as<std::string>();

      if (node["root"])
        m_config.root = node["root"].as<std::string>();

      if (node["scale"])
        m_config.scale = node["scale"].as<float>();

      if (node["luminosity"])
        m_config.luminosity = node["luminosity"].as<float>();
      else {
        throw YAML::ParserException(YAML::Mark::null_mark(), "'configuration' block is missing luminosity");
      }

      if (node["luminosity-error"])
        m_config.luminosity_error_percent = node["luminosity-error"].as<float>();

      if (node["error-fill-color"])
        m_config.error_fill_color = loadColor(node["error-fill-color"]);

      if (node["error-fill-style"])
        m_config.error_fill_style = loadColor(node["error-fill-style"]);

      if (node["ratio-fit-line-style"])
        m_config.ratio_fit_line_style = node["ratio-fit-line-style"].as<int16_t>();

      if (node["ratio-fit-line-width"])
        m_config.ratio_fit_line_width = node["ratio-fit-line-width"].as<int16_t>();

      if (node["ratio-fit-line-color"])
        m_config.ratio_fit_line_color = loadColor(node["ratio-fit-line-color"]);

      if (node["ratio-fit-error-fill-style"])
        m_config.ratio_fit_error_fill_style = node["ratio-fit-error-fill-style"].as<int16_t>();

      if (node["ratio-fit-error-fill-color"])
        m_config.ratio_fit_error_fill_color = loadColor(node["ratio-fit-error-fill-color"]);

      if (node["labels"]) {
        YAML::Node labels = node["labels"];
        m_config.labels = parseLabelsNode(labels);
      }

      m_config.y_axis_format = "%1% / %2$.2f";
      if (node["y-axis-format"])
        m_config.y_axis_format = node["y-axis-format"].as<std::string>();

      if (node["mode"])
          m_config.mode = node["mode"].as<std::string>();

      if (node["tree-name"])
          m_config.tree_name = node["tree-name"].as<std::string>();

      if (node["show-overflow"])
          m_config.show_overflow = node["show-overflow"].as<bool>();

      m_config.errors_type = Poisson;
      if (node["errors-type"])
          m_config.errors_type = string_to_errors_type(node["errors-type"].as<std::string>());

      if (node["ignore-negative-bins"])
          m_config.ignore_negative_bins = node["ignore-negative-bins"].as<bool>();
    }

    // Database
    if (f["database"]) {
        YAML::Node db = f["database"];

        auto config = std::make_shared<sqlpp::mysql::connection_config>();
        config->host = "localhost";
        config->user = "root";
        config->debug = false;

        if (db["host"])
            config->host = db["host"].as<std::string>();

        if (db["user"])
            config->user = db["user"].as<std::string>();

        if (db["password"])
            config->password = db["password"].as<std::string>();

        if (db["database"])
            config->database = db["database"].as<std::string>();

        if (db["debug"])
            config->debug = db["debug"].as<bool>();

        std::cout << "Connecting to SAMADhi..." << std::endl;
        Database::get().connect(config);
        std::cout << "Connected." << std::endl << std::endl;
    }

    YAML::Node files = f["files"];

    for (YAML::const_iterator it = files.begin(); it != files.end(); ++it) {
      File file;

      file.path = it->first.as<std::string>();
      fs::path root = fs::path(m_config.root);
      fs::path path = fs::path(file.path);
      file.path = (root / path).string();

      YAML::Node node = it->second;

      if (node["type"]) {
        std::string type = node["type"].as<std::string>();
        if (type == "signal")
          file.type = SIGNAL;
        else if (type == "data")
          file.type = DATA;
        else
          file.type = MC;
      } else
        file.type = MC;

      if (node["scale"])
        file.scale = node["scale"].as<float>();
      else
        file.scale = 1;

      if (file.type != DATA && Database::get().connected()) {
        file.cross_section = -1.;
        file.generated_events = -1.;
      } else {
        file.cross_section = 1.;
        file.generated_events = 1.;
      }

      if (node["cross-section"])
        file.cross_section = node["cross-section"].as<float>();

      if (node["branching-ratio"])
        file.branching_ratio = node["branching-ratio"].as<float>();
      else
        file.branching_ratio = 1;

      if (node["generated-events"])
        file.generated_events = node["generated-events"].as<float>();

      file.order = std::numeric_limits<int16_t>::min();
      if (node["order"])
        file.order = node["order"].as<int16_t>();

      if (node["group"]) {
        file.group = node["group"].as<std::string>();
      }

      if (node["systematics"]) {

        const auto& addSystematic = [&root, &file](const std::string& systematics) {
          fs::path path = fs::path(systematics);
          std::string fullPath = (root / path).string();

          if (boost::filesystem::exists(fullPath)) {
            Systematic s;
            s.path = fullPath;

            file.systematics.push_back(s);
          } else {
            std::cerr << "Warning: systematics file '" << systematics << "' not found." << std::endl;
          }
        };

        const YAML::Node& syst = node["systematics"];
        if (syst.IsSequence()) {
          for (const auto& it: syst) {
            addSystematic(it.as<std::string>());
          }
        } else {
          addSystematic(syst.as<std::string>());
        }
      }

      file.plot_style = std::make_shared<PlotStyle>();
      file.plot_style->loadFromYAML(node, file, *this);

      // Query the database if needed
      if (file.cross_section < 0 || file.generated_events < 0) {

        if (!node["sample-name"]) {
          throw YAML::ParserException(YAML::Mark::null_mark(), "No cross-section or number of generated events specified for this sample. I would like to retrieve these information from the database, but you did not specified the sample name using the 'sample-name' option.");
        }

        std::string tag = node["sample-name"].as<std::string>();

        if (file.cross_section < 0) {
            file.cross_section = Database::get().get_xsection(tag);
        }

        if (file.generated_events < 0) {
            file.generated_events = Database::get().get_normalization(tag);
        }
      }

      m_files.push_back(file);
    }

    std::sort(m_files.begin(), m_files.end(), [](const File& a, const File& b) {
      return a.order < b.order;
     });

    YAML::Node groups = f["groups"];

    for (YAML::const_iterator it = groups.begin(); it != groups.end(); ++it) {
      Group group;

      group.name = it->first.as<std::string>();

      YAML::Node node = it->second;

      // Find the first file belonging to this group, and use its type to set
      // default style values
      const auto file = std::find_if(m_files.begin(), m_files.end(), [&group](const File& file) {
          return file.group == group.name;
        });

      // Is this group actually used?
      if (file == m_files.end())
          continue;

      group.plot_style = std::make_shared<PlotStyle>();
      group.plot_style->loadFromYAML(node, *file, *this);

      m_groups[group.name] = group;
    }

    // Remove non-existant groups from files
    for (auto& file: m_files) {
      if (!file.group.empty() && !m_groups.count(file.group)) {
        file.group = "";
      }
    }

    if (! f["plots"]) {
      throw YAML::ParserException(YAML::Mark::null_mark(), "You must specify at least one plot in your configuration file");
    }

    YAML::Node plots = f["plots"];

    for (YAML::const_iterator it = plots.begin(); it != plots.end(); ++it) {
      Plot plot;

      plot.name = it->first.as<std::string>();

      YAML::Node node = it->second;
      if (node["exclude"])
        plot.exclude = node["exclude"].as<std::string>();

      if (node["x-axis"])
        plot.x_axis = node["x-axis"].as<std::string>();

      plot.y_axis = "Events";
      if (node["y-axis"])
        plot.y_axis = node["y-axis"].as<std::string>();

      plot.y_axis_format = m_config.y_axis_format;
      if (node["y-axis-format"])
        plot.y_axis_format = node["y-axis-format"].as<std::string>();

      plot.normalized = false;
      if (node["normalized"])
        plot.normalized = node["normalized"].as<bool>();

      Log log_y = False;
      if (node["log-y"]) {
        log_y = parse_log(node["log-y"]);
      }
      if (log_y != Both)
        plot.log_y = (bool) log_y;

      Log log_x = False;
      if (node["log-x"]) {
        log_x = parse_log(node["log-x"]);
      }
      if (log_x != Both)
        plot.log_x = (bool) log_x;

      if (node["save-extensions"])
        plot.save_extensions = node["save-extensions"].as<std::vector<std::string>>();
      else
        plot.save_extensions = {"pdf"};

      if (node["show-ratio"])
        plot.show_ratio = node["show-ratio"].as<bool>();
      else
        plot.show_ratio = false;

      if (node["fit-ratio"])
        plot.fit_ratio = node["fit-ratio"].as<bool>();

      if (node["fit-function"])
        plot.fit_function = node["fit-function"].as<std::string>();

      if (node["fit-legend"])
        plot.fit_legend = node["fit-legend"].as<std::string>();

      if (node["fit-legend-position"])
        plot.fit_legend_position = node["fit-legend-position"].as<Point>();

      if (node["show-errors"])
        plot.show_errors = node["show-errors"].as<bool>();
      else
        plot.show_errors = false;

      if (node["x-axis-range"])
        plot.x_axis_range = node["x-axis-range"].as<std::vector<float>>();

      if (node["y-axis-range"])
        plot.y_axis_range = node["y-axis-range"].as<std::vector<float>>();

      plot.y_axis_show_zero = false;
      if (node["y-axis-show-zero"])
        plot.y_axis_show_zero = node["y-axis-show-zero"].as<bool>();

      if (node["inherits-from"])
        plot.inherits_from = node["inherits-from"].as<std::string>();
      else
        plot.inherits_from = "TH1";

      if (node["rebin"])
        plot.rebin = node["rebin"].as<uint16_t>();
      else
        plot.rebin = 1;

      if (node["labels"]) {
        YAML::Node labels = node["labels"];
        plot.labels = parseLabelsNode(labels);
      }

      if (node["extra-label"])
        plot.extra_label = node["extra-label"].as<std::string>();

      if (node["legend-position"])
        plot.legend_position = node["legend-position"].as<Position>();

      if (node["show-overflow"])
        plot.show_overflow = node["show-overflow"].as<bool>();
      else
        plot.show_overflow = m_config.show_overflow;

      if (node["errors-type"])
        plot.errors_type = string_to_errors_type(node["errors-type"].as<std::string>());
      else
        plot.errors_type = m_config.errors_type;

      plot.ignore_negative_bins = m_config.ignore_negative_bins;
      if (node["ignore-negative-bins"])
        plot.ignore_negative_bins = node["ignore-negative-bins"].as<bool>();

      if (node["binning-x"])
        plot.binning_x = node["binning-x"].as<uint16_t>();

      if (node["binning-y"])
        plot.binning_y = node["binning-y"].as<uint16_t>();

      if (node["draw-string"])
        plot.draw_string = node["draw-string"].as<std::string>();

      if (node["selection-string"])
        plot.selection_string = node["selection-string"].as<std::string>();

      // Handle log
      std::vector<bool> logs_x;
      std::vector<bool> logs_y;

      if (log_x == Both) {
        logs_x.insert(logs_x.end(), {false, true});
      } else {
        logs_x.push_back(plot.log_x);
      }

      if (log_y == Both) {
        logs_y.insert(logs_y.end(), {false, true});
      } else {
        logs_y.push_back(plot.log_y);
      }

      for (auto x: logs_x) {
        for (auto y: logs_y) {
          Plot p = plot;
          p.log_x = x;
          p.log_y = y;

          if (p.log_x)
            p.output_suffix += "_logx";

          if (p.log_y)
            p.output_suffix += "_logy";

          m_plots.push_back(p);
        }
      }
    }

    m_legend.position.x1 = 0.6;
    m_legend.position.y1 = 0.6;

    m_legend.position.x2 = 0.9;
    m_legend.position.y2 = 0.9;
    if (f["legend"]) {
      YAML::Node node = f["legend"];

      if (node["position"])
        m_legend.position = node["position"].as<Position>();
    }

    parseLumiLabel();
  }

  void plotIt::parseLumiLabel() {

    m_config.lumi_label_parsed = m_config.lumi_label;

    float lumi = m_config.luminosity / 1000.;

    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << lumi;
    std::string lumiStr = out.str();

    boost::algorithm::replace_all(m_config.lumi_label_parsed, "%lumi%", lumiStr);
  }

  void plotIt::addToLegend(TLegend& legend, Type type) {
    for (File& file: m_files) {
      if (file.type == type) {
        if (file.group.length() > 0 && m_groups.count(file.group) && m_groups[file.group].plot_style->legend.length() > 0) {
          if (m_groups[file.group].added)
            continue;
          legend.AddEntry(file.object, m_groups[file.group].plot_style->legend.c_str(), m_groups[file.group].plot_style->legend_style.c_str());
          m_groups[file.group].added = true;
        } else if (file.plot_style.get() && file.plot_style->legend.length() > 0) {
          legend.AddEntry(file.object, file.plot_style->legend.c_str(), file.plot_style->legend_style.c_str());
        }
      }
    }
  }

  bool plotIt::plot(Plot& plot) {
    std::cout << "Plotting '" << plot.name << "'" << std::endl;

    bool hasMC = false;
    bool hasData = false;
    bool hasSignal = false;
    bool hasLegend = false;
    // Open all files, and find histogram in each
    for (File& file: m_files) {
      if (! loadObject(file, plot)) {
        return false;
      }

      hasLegend |= getPlotStyle(file)->legend.length() > 0;
      hasData |= file.type == DATA;
      hasMC |= file.type == MC;
      hasSignal |= file.type == SIGNAL;
    }

    // Create canvas
    TCanvas c("canvas", "canvas", m_config.width, m_config.height);

    bool success = ::plotIt::plot(m_files[0], c, plot);

    auto printSummary = [&](Type type) {
      float sum_n_events = 0;
      float sum_n_events_error = 0;

      printf("%50s%18s ± %11s%15s%19s  ± %20s\n", " ", "N", u8"ΔN", " ", u8"ε", u8"Δε");
      for (File& file: m_files) {
        if (file.type == type) {
          fs::path path(file.path);
          printf("%50s%18.2f ± %10.2f%15s%18.5f%% ± %18.5f%%\n", path.stem().c_str(), file.summary.n_events, file.summary.n_events_error, " ", file.summary.efficiency * 100, file.summary.efficiency_error * 100);

          sum_n_events += file.summary.n_events;
          sum_n_events_error += file.summary.n_events_error * file.summary.n_events_error;
        }
      }

      if (sum_n_events) {
        float systematics = 0;
        if (type == MC && m_config.luminosity_error_percent > 0) {
          std::cout << "------------------------------------------" << std::endl;
          std::cout << "Systematic uncertainties" << std::endl;
          printf("%50s%18s ± %10.2f\n", "Luminosity", " ", sum_n_events * m_config.luminosity_error_percent);
          systematics = sum_n_events * m_config.luminosity_error_percent;
        }
        for (File& file: m_files) {
          if (file.type == type) {
            for (Systematic& s: file.systematics) {
              fs::path path(s.path);
              printf("%50s%18s ± %10.2f\n", path.stem().c_str(), " ", s.summary.n_events_error);

              sum_n_events_error += s.summary.n_events_error * s.summary.n_events_error;
            }
          }
        }
        std::cout << "------------------------------------------" << std::endl;
        printf("%50s%18.2f ± %10.2f\n", " ", sum_n_events, sqrt(sum_n_events_error + systematics * systematics));
      }
    };

    if (! success)
      return false;

    std::cout << "Summary: " << std::endl;

    if (hasData) {
     std::cout << "Data" << std::endl;
     printSummary(DATA);
     std::cout << std::endl;
    }

    if (hasMC) {
     std::cout << "MC: " << std::endl;
     printSummary(MC);
     std::cout << std::endl;
    }

    if (hasSignal) {
     std::cout << std::endl << "Signal: " << std::endl;
     printSummary(SIGNAL);
     std::cout << std::endl;
    }

    if (plot.log_y)
      c.SetLogy();

    if (plot.log_x)
      c.SetLogx();

    Position legend_position = m_legend.position;
    if (!plot.legend_position.empty())
      legend_position = plot.legend_position;

    // Build legend
    TLegend legend(legend_position.x1, legend_position.y1, legend_position.x2, legend_position.y2);
    legend.SetTextFont(43);
    legend.SetFillStyle(0);
    legend.SetBorderSize(0);

    addToLegend(legend, MC);
    addToLegend(legend, SIGNAL);
    addToLegend(legend, DATA);

    if (hasMC && plot.show_errors) {
      TLegendEntry* entry = legend.AddEntry("errors", "Uncertainties", "f");
      entry->SetLineWidth(0);
      entry->SetLineColor(m_config.error_fill_color);
      entry->SetFillStyle(m_config.error_fill_style);
      entry->SetFillColor(m_config.error_fill_color);
    }

    legend.Draw();

    float topMargin = TOP_MARGIN;
    if (plot.show_ratio)
      topMargin /= .6666;

    // Move exponent label if shown
    TGaxis::SetExponentOffset(-0.06, 0, "y");

    // Luminosity label
    if (m_config.lumi_label_parsed.length() > 0) {
      std::shared_ptr<TPaveText> pt = std::make_shared<TPaveText>(LEFT_MARGIN, 1 - 0.5 * topMargin, 1 - RIGHT_MARGIN, 1, "brNDC");
      m_temporaryObjects.push_back(pt);

      pt->SetFillStyle(0);
      pt->SetBorderSize(0);
      pt->SetMargin(0);
      pt->SetTextFont(42);
      pt->SetTextSize(0.6 * topMargin);
      pt->SetTextAlign(33);

      pt->AddText(m_config.lumi_label_parsed.c_str());
      pt->Draw();
    }

    // Experiment
    if (m_config.experiment.length() > 0) {
      std::shared_ptr<TPaveText> pt = std::make_shared<TPaveText>(LEFT_MARGIN, 1 - 0.5 * topMargin, 1 - RIGHT_MARGIN, 1, "brNDC");
      m_temporaryObjects.push_back(pt);

      pt->SetFillStyle(0);
      pt->SetBorderSize(0);
      pt->SetMargin(0);
      pt->SetTextFont(62);
      pt->SetTextSize(0.75 * topMargin);
      pt->SetTextAlign(13);

      std::string text = m_config.experiment;
      if (m_config.extra_label.length() || plot.extra_label.length()) {
        std::string extra_label = plot.extra_label;
        if (extra_label.length() == 0) {
          extra_label = m_config.extra_label;
        }

        boost::format fmt("%s #font[52]{#scale[0.76]{%s}}");
        fmt % m_config.experiment % extra_label;

        text = fmt.str();
      }

      pt->AddText(text.c_str());
      pt->Draw();
    }

    c.cd();

    const auto& labels = mergeLabels(plot.labels);

    // Labels
    for (auto& label: labels) {

      std::shared_ptr<TLatex> t(new TLatex(label.position.x, label.position.y, label.text.c_str()));
      t->SetNDC(true);
      t->SetTextFont(43);
      t->SetTextSize(label.size);
      t->Draw();

      m_temporaryObjects.push_back(t);
    }

    std::string plot_name = plot.name + plot.output_suffix;
    boost::replace_all(plot_name, "/", "_");
    fs::path outputName = m_outputPath / plot_name;

    for (const std::string& extension: plot.save_extensions) {
      fs::path outputNameWithExtension = outputName.replace_extension(extension);

      c.SaveAs(outputNameWithExtension.string().c_str());
    }

    // Close all opened files
    m_temporaryObjects.clear();

    // Reset groups
    for (auto& group: m_groups) {
      group.second.added = false;
    }

    // Clear summary
    for (auto& file: m_files) {
      file.summary.clear();
    }

    return true;
  }

  void plotIt::plotAll() {
    // First, explode plots to match all glob patterns

    std::vector<Plot> plots;
    if (m_config.mode == "tree") {
      plots = m_plots;
    } else {
      if (!expandObjects(m_files[0], plots)) {
        return;
      }
    }

    for (Plot& plot: plots) {
      plotIt::plot(plot);
    }
  }

  bool plotIt::loadObject(File& file, const Plot& plot) {

    file.object = nullptr;

    if (m_config.mode == "tree") {

        if (!file.chain.get()) {
          file.chain.reset(new TChain(m_config.tree_name.c_str()));
        }

        file.chain->Add(file.path.c_str());

        std::shared_ptr<TH1> hist(new TH1F(plot.name.c_str(), "", plot.binning_x, plot.x_axis_range[0], plot.x_axis_range[1]));
        hist->GetDirectory()->cd();

        file.chain->Draw((plot.draw_string + ">>" + plot.name).c_str(), plot.selection_string.c_str());

        hist->SetDirectory(nullptr);
        file.object = hist.get();

        m_temporaryObjects.push_back(hist);

        return true;
    }

    std::shared_ptr<TFile> input(TFile::Open(file.path.c_str()));
    if (! input.get())
      return false;

    TObject* obj = input->Get(plot.name.c_str());

    if (obj) {
      m_temporaryObjects.push_back(input);
      file.object = obj;

      // Load systematics histograms
      for (Systematic& syst: file.systematics) {

        syst.object = nullptr;

        std::shared_ptr<TFile> input_syst(TFile::Open(syst.path.c_str()));
        if (! input_syst.get())
          continue;

        obj = input_syst->Get(plot.name.c_str());
        if (obj) {
          m_temporaryObjects.push_back(input_syst);
          syst.object = obj;
        }
      }

      return true;
    }

    // Should not be possible!
    std::cout << "Error: object '" << plot.name << "' inheriting from '" << plot.inherits_from << "' not found in file '" << file.path << "'" << std::endl;
    return false;
  }

  bool plotIt::expandFiles() {
    std::vector<File> files;

    for (File& file: m_files) {
      std::vector<std::string> matchedFiles = glob(file.path);
      for (std::string& matchedFile: matchedFiles) {
        File f = file;
        f.path = matchedFile;
        //std::cout << file.path << " matches to " << f.path << std::endl;

        files.push_back(f);
      }
    }

    m_files = files;

    return true;
  }

  /**
   * Merge the labels of the global configuration and the current plot.
   * If some are duplicated, only keep the plot label
   **/
  std::vector<Label> plotIt::mergeLabels(const std::vector<Label>& plotLabels) {
    std::vector<Label> labels = plotLabels;

    // Add labels from global configuration, and check for duplicates
    for (auto& globalLabel: m_config.labels) {

      bool duplicated = false;
      for (auto& label: plotLabels) {
        if (globalLabel.text == label.text) {
          duplicated = true;
          break;
        }
      }

      if (! duplicated)
        labels.push_back(globalLabel);
    }

    return labels;
  }

  /**
   * Open 'file', and expand all plots
   */
  bool plotIt::expandObjects(File& file, std::vector<Plot>& plots) {
    file.object = nullptr;
    plots.clear();

    std::shared_ptr<TFile> input(TFile::Open(file.path.c_str()));
    if (! input.get())
      return false;

    TIter root_keys(input->GetListOfKeys());

    for (Plot& plot: m_plots) {
      TKey* key;
      TObject* obj;
      bool match = false;

      std::vector<std::string> tokens;
      boost::split(tokens, plot.name, boost::is_any_of("/"));

      bool in_directory = tokens.size() != 1;
      std::string plot_name = tokens.back();
      std::string root_name;

      root_keys.Reset();

      TIter it = root_keys;

      if (in_directory) {
          auto find_folder = [&](const std::string& name, TDirectory* root) -> TDirectory* {
              TIter it(root->GetListOfKeys());
              while ((key = static_cast<TKey*>(it()))) {
                  TObject* obj = key->ReadObj();
                  if (!obj->InheritsFrom("TDirectory"))
                      continue;

                  if (fnmatch(name.c_str(), obj->GetName(), FNM_CASEFOLD) == 0) {
                      return dynamic_cast<TDirectory*>(obj);
                  }
              }

              return nullptr;
          };

          std::string not_found;
          TDirectory* root = input.get();
          for (size_t i = 0; i < tokens.size() - 1; i++) {
              std::string folder = tokens[i];
              root = find_folder(folder, root);

              if (! root) {
                  not_found = folder;
                  break;
              }

              root_name += std::string(root->GetName()) + "/";
          }

          if (! root) {
              std::cout << "Warning: The folder '" << not_found << "' was not found in file '" << file.path << "'" << std::endl;
              continue;
          }

          it = TIter(root->GetListOfKeys());
      }

      std::vector<std::string> matched;
      while ((key = static_cast<TKey*>(it()))) {
        obj = key->ReadObj();
        if (! obj->InheritsFrom(plot.inherits_from.c_str()))
          continue;

        // Check name
        if (fnmatch(plot_name.c_str(), obj->GetName(), FNM_CASEFOLD) == 0) {

          // Check if this name is excluded
          if ((plot.exclude.length() > 0) && (fnmatch(plot.exclude.c_str(), obj->GetName(), FNM_CASEFOLD) == 0)) {
            continue;
          }

          std::string expanded_plot_name = root_name + obj->GetName();

          // The same object can be stored multiple time with a different key
          // The iterator returns first the object with the highest key, which is the most recent object
          // Check if we already have a plot with the same exact name
          if (std::find_if(matched.begin(), matched.end(), [&expanded_plot_name](const std::string& p) { return p == expanded_plot_name; }) != matched.end()) {
            continue;
          }

          // Got it!
          match = true;
          matched.push_back(expanded_plot_name);
          plots.push_back(plot.Clone(expanded_plot_name));
        }
      }

      if (! match) {
        std::cout << "Warning: object '" << plot.name << "' inheriting from '" << plot.inherits_from << "' does not match something in file '" << file.path << "'" << std::endl;
      }
    }

    if (!plots.size()) {
      std::cout << "Error: no plots found in file '" << file.path << "'" << std::endl;
      return false;
    }

    return true;
  }

  std::shared_ptr<PlotStyle> plotIt::getPlotStyle(const File& file) {
    if (file.group.length() && m_groups.count(file.group)) {
      return m_groups[file.group].plot_style;
    } else {
      return file.plot_style;
    }
  }

  void PlotStyle::loadFromYAML(YAML::Node& node, const File& file, plotIt& pIt) {
    if (node["legend"])
      legend = node["legend"].as<std::string>();

    if (file.type == MC)
      legend_style = "lf";
    else if (file.type == SIGNAL)
      legend_style = "l";
    else if (file.type == DATA)
      legend_style = "p";

    if (node["legend-style"])
      legend_style = node["legend-style"].as<std::string>();

    if (node["drawing-options"])
      drawing_options = node["drawing-options"].as<std::string>();
    else {
      if (file.type == MC || file.type == SIGNAL)
        drawing_options = "hist";
      else if (file.type == DATA)
        drawing_options = "P";
    }

    marker_size = -1;
    marker_color = -1;
    marker_type = -1;

    fill_color = -1;
    fill_type = -1;

    line_color = -1;
    line_type = -1;

    if (file.type == MC) {
      fill_color = 1;
      fill_type = 1001;
      line_width = 0;
    } else if (file.type == SIGNAL) {
      fill_type = 0;
      line_color = 1;
      line_width = 1;
      line_type = 2;
    } else {
      marker_size = 1;
      marker_color = 1;
      marker_type = 20;
      line_color = 1;
      line_width = 1; // For uncertainties
    }

    if (node["fill-color"])
      fill_color = pIt.loadColor(node["fill-color"]);

    if (node["fill-type"])
      fill_type = node["fill-type"].as<int16_t>();

    if (node["line-color"])
      line_color = pIt.loadColor(node["line-color"]);

    if (node["line-type"])
      line_type = node["line-type"].as<int16_t>();

    if (node["line-width"])
      line_width = node["line-width"].as<float>();

    if (node["marker-color"])
      marker_color = pIt.loadColor(node["marker-color"]);

    if (node["marker-type"])
      marker_type = node["marker-type"].as<int16_t>();

    if (node["marker-size"])
      marker_size = node["marker-size"].as<float>();
  }
}

int main(int argc, char** argv) {

  try {

    TCLAP::CmdLine cmd("Plot histograms", ' ', "0.1");

    TCLAP::ValueArg<std::string> outputFolderArg("o", "output-folder", "output folder", true, "", "string", cmd);

    TCLAP::SwitchArg ignoreScaleArg("", "ignore-scales", "Ignore any scales present in the configuration file", cmd, false);

    TCLAP::UnlabeledValueArg<std::string> configFileArg("configFile", "configuration file", true, "", "string", cmd);

    cmd.parse(argc, argv);

    //bool isData = dataArg.isSet();

    fs::path outputPath(outputFolderArg.getValue());

    if (! fs::exists(outputPath)) {
      std::cout << "Error: output path " << outputPath << " does not exist" << std::endl;
      return 1;
    }

    plotIt::plotIt p(outputPath, configFileArg.getValue());
    p.getConfigurationForEditing().ignore_scales = ignoreScaleArg.getValue();

    p.plotAll();

  } catch (TCLAP::ArgException &e) {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    return 1;
  }

  return 0;
}
