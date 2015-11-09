#pragma once

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
#include <memory>
#include <iomanip>
#include <iostream>

#include "yaml-cpp/yaml.h"

#include <TH1.h>
#include <THStack.h>
#include <TStyle.h>
#include <TChain.h>

#include <vector>
#include <string>
#include <glob.h>

#include <defines.h>

namespace YAML {
  class Node;
}

class TFile;
class TObject;
class TCanvas;
class TLegend;

namespace fs = boost::filesystem;

namespace plotIt {

  enum Type {
    MC,
    SIGNAL,
    DATA
  };

  enum ErrorsType {
      Normal = 0,
      Poisson = 1,
      Poisson2 = 2
  };

  inline ErrorsType string_to_errors_type(const std::string& s) {
      ErrorsType errors_type;

      if (s == "normal")
          errors_type = Normal;
      else if (s == "poisson2")
          errors_type = Poisson2;
      else 
          errors_type = Poisson;

      return errors_type;
  }

  enum Log {
    False = 0,
    True,
    Both
  };

  inline Log parse_log(const YAML::Node& node) {
    if (node.as<std::string>() == "both")
      return Both;
    else
      return node.as<bool>() ? True : False;
  }

  struct PlotStyle;
  class plotIt;
  struct Group;

  struct Summary {
    float n_events;
    float n_events_error;

    float efficiency;
    float efficiency_error;

    Summary() {
      n_events = n_events_error = efficiency = efficiency_error = 0;
    }

    void clear() {
      n_events = n_events_error = efficiency = efficiency_error = 0;
    }
  };

  struct Systematic {
    std::string path;
    TObject* object;

    Summary summary;
  };

  struct File {
    std::string path;

    // For MC and Signal
    float cross_section;
    float branching_ratio;
    float generated_events;
    float scale;

    // For Data
    float luminosity;

    std::shared_ptr<PlotStyle> plot_style;
    std::string group;

    Type type;

    TObject* object;

    std::vector<Systematic> systematics;

    int16_t order;
    Summary summary;

    std::shared_ptr<TChain> chain;
  };

  struct PlotStyle {

    // Style
    float marker_size;
    int16_t marker_color;
    int16_t marker_type;
    int16_t fill_color;
    int16_t fill_type;
    float line_width;
    int16_t line_color;
    int16_t line_type;
    std::string drawing_options;

    // Legend
    std::string legend;
    std::string legend_style;

    void loadFromYAML(YAML::Node& node, const File& file, plotIt& pIt);
  };

  struct Group {
    std::string name;
    std::shared_ptr<PlotStyle> plot_style;

    bool added = false;
  };

  struct Point {
    float x;
    float y;

    bool operator==(const Point& other) {
      return
        (fabs(x - other.x) < 1e-6) &&
        (fabs(y - other.y) < 1e-6);
    }
  };

  struct Position {
    float x1 = 0;
    float y1 = 0;

    float x2 = 0;
    float y2 = 0;

    bool operator==(const Position& other) {
      return
        (fabs(x1 - other.x1) < 1e-6) &&
        (fabs(y1 - other.y1) < 1e-6) &&
        (fabs(x2 - other.x2) < 1e-6) &&
        (fabs(y2 - other.y2) < 1e-6);
    }

    bool empty() const {
      return
        (x1 == 0) &&
        (x2 == 0) &&
        (y1 == 0) &&
        (y2 == 0);
    }
  };

  struct Label {
    std::string text;
    uint32_t size = LABEL_FONTSIZE;
    Point position;
  };

  struct Plot {
    std::string name;
    std::string output_suffix;
    std::string exclude;

    bool normalized;
    bool log_y;
    bool log_x;

    std::string x_axis;
    std::string y_axis;
    std::string y_axis_format;

    // Axis range
    std::vector<float> x_axis_range;
    std::vector<float> y_axis_range;

    uint16_t binning_x;  // Only used in tree mode
    uint16_t binning_y;  // Only used in tree mode

    std::string draw_string;  // Only used in tree mode
    std::string selection_string;  // Only used in tree mode

    bool y_axis_show_zero;

    std::vector<std::string> save_extensions;

    bool show_ratio;
    bool fit_ratio = false;
    std::string fit_function = "pol1";
    std::string fit_legend;
    Point fit_legend_position = {0.20, 0.38};

    bool show_errors;
    bool show_overflow = false;
    bool ignore_negative_bins = false;

    std::string inherits_from;

    uint16_t rebin;

    std::vector<Label> labels;

    std::string extra_label;

    Position legend_position;

    ErrorsType errors_type = Poisson;

    void print() {
      std::cout << "Plot '" << name << "'" << std::endl;
      std::cout << "\tx_axis: " << x_axis << std::endl;
      std::cout << "\ty_axis: " << y_axis << std::endl;
      std::cout << "\tshow_ratio: " << show_ratio << std::endl;
      std::cout << "\tinherits_from: " << inherits_from << std::endl;
      std::cout << "\tsave_extensions: " << boost::algorithm::join(save_extensions, ", ") << std::endl;
    }

    Plot Clone(const std::string& new_name) {
      Plot clone = *this;
      clone.name = new_name;

      return clone;
    }
  };

  struct Legend {
    Position position;
  };

  struct Configuration {
    float width;
    float height;
    float luminosity;
    float scale;

    // Systematics
    float luminosity_error_percent;

    std::string y_axis_format;

    int16_t error_fill_color;
    int16_t error_fill_style;

    int16_t ratio_fit_line_color = 46;
    int16_t ratio_fit_line_width = 1;
    int16_t ratio_fit_line_style = 1;
    int16_t ratio_fit_error_fill_color = 42;
    int16_t ratio_fit_error_fill_style = 1001;

    std::vector<Label> labels;

    std::string experiment = "CMS";
    std::string extra_label;

    std::string lumi_label;
    std::string lumi_label_parsed;

    std::string root;

    bool ignore_scales = false;
    bool show_overflow = false;
    bool ignore_negative_bins = false;

    std::string mode = "hist"; // "tree" or "hist"
    std::string tree_name;

    ErrorsType errors_type = Poisson;

    Configuration() {
      width = height = 800;
      root = "./";
      luminosity = - 1;
      scale = 1.;
      luminosity_error_percent = 0;
      error_fill_color = 42;
      error_fill_style = 3154;
    }
  };

  class plotIt {
    public:
      plotIt(const fs::path& outputPath, const std::string& configFile);
      void plotAll();

      std::vector<File>& getFiles() {
        return m_files;
      }

      Configuration getConfiguration() const {
        return m_config;
      }

      Configuration& getConfigurationForEditing() {
        return m_config;
      }

      void addTemporaryObject(const std::shared_ptr<TObject>& object) {
        m_temporaryObjects.push_back(object);
      }

      std::shared_ptr<PlotStyle> getPlotStyle(const File& file);

      friend PlotStyle;

    private:
      void checkOrThrow(YAML::Node& node, const std::string& name, const std::string& file);
      void parseConfigurationFile(const std::string& file);
      void parseIncludes(YAML::Node& node);
      int16_t loadColor(const YAML::Node& node);

      // Plot method
      bool plot(Plot& plot);

      bool expandFiles();
      bool expandObjects(File& file, std::vector<Plot>& plots);
      bool loadObject(File& file, const Plot& plot);

      void addToLegend(TLegend& legend, Type type);

      void parseLumiLabel();

      std::vector<Label> mergeLabels(const std::vector<Label>& labels);

      fs::path m_outputPath;

      std::vector<File> m_files;
      std::vector<Plot> m_plots;
      std::map<std::string, Group> m_groups;

      // Store objects in order to delete everything when drawing is done
      std::vector<std::shared_ptr<TObject>> m_temporaryObjects;

      // Temporary object living the whole runtime
      std::vector<std::shared_ptr<TObject>> m_temporaryObjectsRuntime;

      // Current style
      std::shared_ptr<TStyle> m_style;

      //float m_luminosity;

      Legend m_legend;
      Configuration m_config;

      // For colors
      uint32_t m_colorIndex = 1000;
  };
};

inline std::vector<std::string> glob(const std::string& pat) {
  glob_t glob_result;
  glob(pat.c_str(), GLOB_TILDE, NULL, &glob_result);

  std::vector<std::string> ret;
  for(unsigned int i = 0;i < glob_result.gl_pathc; ++i){
    ret.push_back(std::string(glob_result.gl_pathv[i]));
  }

  globfree(&glob_result);
  return ret;
}

namespace YAML {
  template<>
    struct convert<plotIt::Position> {
      static Node encode(const plotIt::Position& rhs) {
        Node node;
        node.push_back(rhs.x1);
        node.push_back(rhs.y1);
        node.push_back(rhs.x1);
        node.push_back(rhs.y1);

        return node;
      }

      static bool decode(const Node& node, plotIt::Position& rhs) {
        if(!node.IsSequence() || node.size() != 4)
          return false;

        rhs.x1 = node[0].as<float>();
        rhs.y1 = node[1].as<float>();
        rhs.x2 = node[2].as<float>();
        rhs.y2 = node[3].as<float>();

        return true;
      }
    };

  template<>
    struct convert<plotIt::Point> {
      static Node encode(const plotIt::Point& rhs) {
        Node node;
        node.push_back(rhs.x);
        node.push_back(rhs.y);

        return node;
      }

      static bool decode(const Node& node, plotIt::Point& rhs) {
        if(!node.IsSequence() || node.size() != 2)
          return false;

        rhs.x = node[0].as<float>();
        rhs.y = node[1].as<float>();

        return true;
      }
    };
}

