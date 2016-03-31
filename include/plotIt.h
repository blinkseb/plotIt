#pragma once

#include <boost/filesystem.hpp>
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
#include <unordered_map>

#include <types.h>
#include <defines.h>
#include <uuid.h>

namespace YAML {
  class Node;
}

class TFile;
class TObject;
class TCanvas;
class TLegend;

namespace fs = boost::filesystem;

namespace plotIt {

  class plotIt {
    public:
      plotIt(const fs::path& outputPath);
      void parseConfigurationFile(const std::string& file);
      void plotAll();

      std::vector<File>& getFiles() {
        return m_files;
      }

      Configuration getConfiguration() const {
        return m_config;
      }

      std::shared_ptr<PlotStyle> getPlotStyle(const File& file);

      friend PlotStyle;

    private:
      void checkOrThrow(YAML::Node& node, const std::string& name, const std::string& file);
      void parseIncludes(YAML::Node& node);
      void parseSystematicsNode(const YAML::Node& node);

      // Plot method
      bool plot(Plot& plot);
      bool yields(std::vector<Plot>& plots);

      bool expandFiles();
      bool expandObjects(File& file, std::vector<Plot>& plots);
      bool loadAllObjects(File& file, const std::vector<Plot>& plots);
      bool loadObject(File& file, const Plot& plot);

      void fillLegend(TLegend& legend, const Plot& plot, bool with_uncertainties);

      void parseLumiLabel();

      std::vector<Label> mergeLabels(const std::vector<Label>& labels);

      fs::path m_outputPath;

      std::vector<File> m_files;
      std::vector<Plot> m_plots;
      std::vector<SystematicPtr> m_systematics;
      std::map<std::string, Group> m_legend_groups;
      std::map<std::string, Group> m_yields_groups;

      std::unordered_map<std::string, TDirectory*> m_book_keeping_folders;

      // Current style
      std::shared_ptr<TStyle> m_style;

      Legend m_legend;
      Configuration m_config;
  };
}
