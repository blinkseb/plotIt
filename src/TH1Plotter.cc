#include <TH1Plotter.h>

#include <TCanvas.h>
#include <TF1.h>
#include <TLatex.h>
#include <TObject.h>
#include <TVirtualFitter.h>
#include <TGraphAsymmErrors.h>

#include <utilities.h>

namespace plotIt {

    /*!
     * Compute the ratio between two histograms, taking into account asymetric error bars
     */
    std::shared_ptr<TGraphAsymmErrors> getRatio(TH1* a, TH1* b) {
        std::shared_ptr<TGraphAsymmErrors> g(new TGraphAsymmErrors(a));

        size_t npoint = 0;
        for (size_t i = 1; i <= (size_t) a->GetNbinsX(); i++) {
            float b1 = a->GetBinContent(i);
            float b2 = b->GetBinContent(i);

            if ((b1 == 0) || (b2 == 0))
                continue;
            
            float ratio = b1 / b2;

            float b1sq = b1 * b1;
            float b2sq = b2 * b2;

            float e1sq_up = a->GetBinErrorUp(i) * a->GetBinErrorUp(i);
            float e2sq_up = b->GetBinErrorUp(i) * b->GetBinErrorUp(i);

            float e1sq_low = a->GetBinErrorLow(i) * a->GetBinErrorLow(i);
            float e2sq_low = b->GetBinErrorLow(i) * b->GetBinErrorLow(i);

            float error_up = sqrt((e1sq_up * b2sq + e2sq_up * b1sq) / (b2sq * b2sq));
            float error_low = sqrt((e1sq_low * b2sq + e2sq_low * b1sq) / (b2sq * b2sq));

            //Set the point center and its errors
            g->SetPoint(npoint, a->GetBinCenter(i), ratio);
            //g->SetPointError(npoint, a->GetBinCenter(i) - a->GetBinLowEdge(i),
                    //a->GetBinLowEdge(i) - a->GetBinCenter(i) + a->GetBinWidth(i),
                    //error_low, error_up);
            g->SetPointError(npoint, 0, 0, error_low, error_up);
            npoint++; 
        }

        g->Set(npoint);

        return g;
    }

  bool TH1Plotter::supports(TObject& object) {
    return object.InheritsFrom("TH1");
  }

  bool TH1Plotter::plot(TCanvas& c, Plot& plot) {
    c.cd();

    // Rescale and style histograms
    for (File& file: m_plotIt.getFiles()) {
      setHistogramStyle(file);

      TH1* h = dynamic_cast<TH1*>(file.object);

      if (file.type != DATA) {
        float factor = m_plotIt.getConfiguration().luminosity * file.cross_section * file.branching_ratio / file.generated_events;
        if (! m_plotIt.getConfiguration().ignore_scales) {
          factor *= m_plotIt.getConfiguration().scale * file.scale;
        }

        float n_entries = h->Integral();
        file.summary.efficiency = n_entries / file.generated_events;
        file.summary.efficiency_error = sqrt( (file.summary.efficiency * (1 - file.summary.efficiency)) / file.generated_events );

        file.summary.n_events = n_entries * factor;
        file.summary.n_events_error = m_plotIt.getConfiguration().luminosity * file.cross_section * file.branching_ratio * file.summary.efficiency_error;
        if (! m_plotIt.getConfiguration().ignore_scales) {
          file.summary.n_events_error *= m_plotIt.getConfiguration().scale * file.scale;
        }

        h->Scale(factor);
      } else {
        file.summary.n_events = h->Integral();
        file.summary.n_events_error = 0;
      }

      h->Rebin(plot.rebin);

      // Add overflow to first and last bin if requested
      if (plot.show_overflow) {
        addOverflow(h, file.type, plot);
      }

      if (plot.ignore_negative_bins) {
        removeNegativeBins(h);
      }

      for (Systematic& s: file.systematics) {
        TH1* syst = static_cast<TH1*>(s.object);
        syst->Rebin(plot.rebin);
        if (plot.show_overflow) {
          addOverflow(syst, file.type, plot);
        }

        if (plot.ignore_negative_bins) {
          removeNegativeBins(syst);
        }
      }
    }

    // Build a THStack for MC files and a vector for signal
    float mcWeight = 0;
    std::shared_ptr<THStack> mc_stack;
    std::shared_ptr<TH1> mc_histo_stat_only;
    std::shared_ptr<TH1> mc_histo_syst_only;
    std::shared_ptr<TH1> mc_histo_stat_syst;

    std::shared_ptr<TH1> h_data;
    std::string data_drawing_options;

    std::vector<File> signal_files;

    for (File& file: m_plotIt.getFiles()) {
      if (file.type == MC) {

        TH1* nominal = dynamic_cast<TH1*>(file.object);

        if (mc_stack.get() == nullptr)
          mc_stack = std::make_shared<THStack>("mc_stack", "mc_stack");

        mc_stack->Add(nominal, m_plotIt.getPlotStyle(file)->drawing_options.c_str());
        if (mc_histo_stat_only.get()) {
          mc_histo_stat_only->Add(nominal);
        } else {
          mc_histo_stat_only.reset( dynamic_cast<TH1*>(nominal->Clone()) );
          mc_histo_stat_only->SetDirectory(nullptr);
        }
        mcWeight += nominal->GetSumOfWeights();

      } else if (file.type == SIGNAL) {
        signal_files.push_back(file);
      } else if (file.type == DATA) {
        if (! h_data.get()) {
          h_data.reset(dynamic_cast<TH1*>(file.object->Clone()));
          h_data->SetDirectory(nullptr);
          h_data->Sumw2(false); // Disable SumW2 for data
          h_data->SetBinErrorOption((TH1::EBinErrorOpt) plot.errors_type);
          data_drawing_options += m_plotIt.getPlotStyle(file)->drawing_options;
        } else {
          h_data->Add(dynamic_cast<TH1*>(file.object));
        }
      }
    }

    if ((h_data.get()) && !h_data->GetSumOfWeights())
      h_data.reset();

    if ((mc_histo_stat_only.get() && !mc_histo_stat_only->GetSumOfWeights())) {
      mc_histo_stat_only.reset();
      mc_stack.reset();
    }

    if (plot.normalized) {
      // Normalized each plot
      for (File& file: m_plotIt.getFiles()) {
        TH1* h = dynamic_cast<TH1*>(file.object);
        if (file.type == MC) {
          h->Scale(1. / fabs(mcWeight));
        } else if (file.type == SIGNAL) {
          h->Scale(1. / fabs(h->GetSumOfWeights()));
        }
      }

      if (h_data.get()) {
        h_data->Scale(1. / h_data->GetSumOfWeights());
      }
    }

    if (mc_histo_stat_only.get()) {
      mc_histo_syst_only.reset(static_cast<TH1*>(mc_histo_stat_only->Clone()));
      mc_histo_syst_only->SetDirectory(nullptr);
      mc_histo_stat_syst.reset(static_cast<TH1*>(mc_histo_stat_only->Clone()));
      mc_histo_stat_syst->SetDirectory(nullptr);

      // Clear statistical errors
      for (uint32_t i = 1; i <= (uint32_t) mc_histo_syst_only->GetNbinsX(); i++) {
        mc_histo_syst_only->SetBinError(i, 0);
      }
    }

    if (mc_histo_syst_only.get() && plot.show_errors) {
      if (m_plotIt.getConfiguration().luminosity_error_percent > 0) {
        // Loop over all bins, and add lumi error
        for (uint32_t i = 1; i <= (uint32_t) mc_histo_syst_only->GetNbinsX(); i++) {
          float error = mc_histo_syst_only->GetBinError(i);
          float entries = mc_histo_syst_only->GetBinContent(i);
          float lumi_error = entries * m_plotIt.getConfiguration().luminosity_error_percent;

          mc_histo_syst_only->SetBinError(i, std::sqrt(error * error + lumi_error * lumi_error));
        }
      }

      // Check if systematic histogram are attached, and add them to the plot
      for (File& file: m_plotIt.getFiles()) {
        if (file.type != MC || file.systematics.size() == 0)
          continue;

        TH1* nominal = dynamic_cast<TH1*>(file.object);

        for (Systematic& syst: file.systematics) {
          // This histogram should contains syst errors
          // in percent
          float total_syst_error = 0;
          TH1* h = dynamic_cast<TH1*>(syst.object);
          for (uint32_t i = 1; i <= (uint32_t) mc_histo_syst_only->GetNbinsX(); i++) {
            float total_error = mc_histo_syst_only->GetBinError(i);
            float syst_error_percent = h->GetBinError(i);
            float nominal_value = nominal->GetBinContent(i);
            float syst_error = nominal_value * syst_error_percent;
            total_syst_error += syst_error;

            mc_histo_syst_only->SetBinError(i, std::sqrt(total_error * total_error + syst_error * syst_error));
          }

          syst.summary.n_events = file.summary.n_events;
          syst.summary.n_events_error = total_syst_error;
        }
      }

      // Propagate syst errors to the stat + syst histogram
      for (uint32_t i = 1; i <= (uint32_t) mc_histo_syst_only->GetNbinsX(); i++) {
        float syst_error = mc_histo_syst_only->GetBinError(i);
        float stat_error = mc_histo_stat_only->GetBinError(i);
        mc_histo_stat_syst->SetBinError(i, std::sqrt(syst_error * syst_error + stat_error * stat_error));
      }
    }

    // Store all the histograms to draw, and find the one with the highest maximum
    std::vector<std::pair<TObject*, std::string>> toDraw = { std::make_pair(mc_stack.get(), ""), std::make_pair(h_data.get(), data_drawing_options) };
    for (File& signal: signal_files) {
      toDraw.push_back(std::make_pair(signal.object, m_plotIt.getPlotStyle(signal)->drawing_options));
    }

    // Remove NULL items
    toDraw.erase(
        std::remove_if(toDraw.begin(), toDraw.end(), [](std::pair<TObject*, std::string>& element) {
          return element.first == nullptr;
        }), toDraw.end()
      );

    if (!toDraw.size()) {
      std::cerr << "Error: nothing to draw." << std::endl;
      return false;
    };

    // Sort object by minimum
    std::sort(toDraw.begin(), toDraw.end(), [](std::pair<TObject*, std::string> a, std::pair<TObject*, std::string> b) {
        return getMinimum(a.first) < getMinimum(b.first);
      });

    float minimum = getMinimum(toDraw[0].first);

    // Sort objects by maximum
    std::sort(toDraw.begin(), toDraw.end(), [](std::pair<TObject*, std::string> a, std::pair<TObject*, std::string> b) {
        return getMaximum(a.first) > getMaximum(b.first);
      });

    float maximum = getMaximum(toDraw[0].first);

    if ((!h_data.get() || !mc_histo_stat_only.get()))
      plot.show_ratio = false;

    std::shared_ptr<TPad> hi_pad;
    std::shared_ptr<TPad> low_pad;
    if (plot.show_ratio) {
      hi_pad = std::make_shared<TPad>("pad_hi", "", 0., 0.33333, 1, 1);
      hi_pad->Draw();
      hi_pad->SetTopMargin(TOP_MARGIN / .6666);
      hi_pad->SetLeftMargin(LEFT_MARGIN);
      hi_pad->SetBottomMargin(0.015);
      hi_pad->SetRightMargin(RIGHT_MARGIN);

      low_pad = std::make_shared<TPad>("pad_lo", "", 0., 0., 1, 0.33333);
      low_pad->Draw();
      low_pad->SetLeftMargin(LEFT_MARGIN);
      low_pad->SetTopMargin(1.);
      low_pad->SetBottomMargin(BOTTOM_MARGIN / .3333);
      low_pad->SetRightMargin(RIGHT_MARGIN);
      low_pad->SetTickx(1);

      hi_pad->cd();
      if (plot.log_y)
        hi_pad->SetLogy();

      if (plot.log_x) {
        hi_pad->SetLogx();
        low_pad->SetLogx();
      }
    }

    toDraw[0].first->Draw(toDraw[0].second.c_str());
    setRange(toDraw[0].first, plot);

    float safe_margin = .20;
    if (plot.log_y)
      safe_margin = 8;

    if (plot.y_axis_range.size() != 2) {
      setMaximum(toDraw[0].first, maximum * (1 + safe_margin));

      if (minimum <= 0 && plot.log_y) {
        std::cout << "Warning: detected minimum is negative (" << minimum << ") but log scale is on. Setting minimum to 0.1" << std::endl;
        minimum = 0.1;
      }

      if (!plot.log_y)
        minimum = minimum * (1 - std::copysign(safe_margin, minimum));
      else
        minimum /= safe_margin;

      if (plot.y_axis_show_zero && !plot.log_y)
        minimum = 0;

      setMinimum(toDraw[0].first, minimum);
    }

    // First, draw MC
    if (mc_stack.get()) {
      mc_stack->Draw("same");
      m_plotIt.addTemporaryObject(mc_stack);
    }

    // Then, if requested, errors
    if (mc_histo_stat_syst.get() && plot.show_errors) {
      mc_histo_stat_syst->SetMarkerSize(0);
      mc_histo_stat_syst->SetMarkerStyle(0);
      mc_histo_stat_syst->SetFillStyle(m_plotIt.getConfiguration().error_fill_style);
      mc_histo_stat_syst->SetFillColor(m_plotIt.getConfiguration().error_fill_color);

      mc_histo_stat_syst->Draw("E2 same");
      m_plotIt.addTemporaryObject(mc_histo_stat_syst);
    }

    // Then signal
    for (File& signal: signal_files) {
      std::string options = m_plotIt.getPlotStyle(signal)->drawing_options + " same";
      signal.object->Draw(options.c_str());
    }

    // And finally data
    if (h_data.get()) {
      data_drawing_options += " E X0 same";
      h_data->Draw(data_drawing_options.c_str());
      m_plotIt.addTemporaryObject(h_data);
    }
    
    // Set x and y axis titles, and default style
    for (auto& obj: toDraw) {
      setDefaultStyle(obj.first, (plot.show_ratio) ? 0.6666 : 1.);
      setAxisTitles(obj.first, plot);
    }

    // Redraw only axis
    toDraw[0].first->Draw("axis same");

    if (plot.show_ratio) {

      // Compute ratio and draw it
      low_pad->cd();
      low_pad->SetGridy();

      std::shared_ptr<TH1> h_low_pad_axis(static_cast<TH1*>(h_data->Clone()));
      h_low_pad_axis->SetDirectory(nullptr);
      h_low_pad_axis->Reset(); // Keep binning
      h_low_pad_axis->SetMaximum(2);
      h_low_pad_axis->SetMinimum(0);
      setRange(h_low_pad_axis.get(), plot);

      setDefaultStyle(h_low_pad_axis.get(), 1. / 0.3333);
      h_low_pad_axis->GetYaxis()->SetTickLength(0.04);
      h_low_pad_axis->GetYaxis()->SetNdivisions(505, true);
      h_low_pad_axis->GetXaxis()->SetTickLength(0.07);

      h_low_pad_axis->Draw("P E X0");

      std::shared_ptr<TGraphAsymmErrors> ratio = getRatio(h_data.get(), mc_histo_stat_only.get());
      ratio->Draw("P same");

      // Compute systematic errors in %
      std::shared_ptr<TH1> h_systematics(static_cast<TH1*>(h_low_pad_axis->Clone()));
      h_systematics->SetDirectory(nullptr);
      h_systematics->Reset(); // Keep binning
      h_systematics->SetMarkerSize(0);

      bool has_syst = false;
      for (uint32_t i = 1; i <= (uint32_t) h_systematics->GetNbinsX(); i++) {

        if (mc_histo_syst_only->GetBinContent(i) == 0 || mc_histo_syst_only->GetBinError(i) == 0)
          continue;

        float syst = mc_histo_syst_only->GetBinContent(i) / (mc_histo_syst_only->GetBinError(i) + mc_histo_syst_only->GetBinContent(i));
        h_systematics->SetBinContent(i, 1);
        h_systematics->SetBinError(i, 1 - syst);

        has_syst = true;
      }

      if (has_syst) {
        h_systematics->SetFillStyle(m_plotIt.getConfiguration().error_fill_style);
        h_systematics->SetFillColor(m_plotIt.getConfiguration().error_fill_color);
        setRange(h_systematics.get(), plot, true);
        h_systematics->Draw("E2");
      }

      h_low_pad_axis->Draw("P E X0 same");

      if (plot.fit_ratio) {
        float xMin = h_low_pad_axis->GetXaxis()->GetBinLowEdge(1);
        float xMax = h_low_pad_axis->GetXaxis()->GetBinUpEdge(h_low_pad_axis->GetXaxis()->GetLast());
        std::shared_ptr<TF1> fct = std::make_shared<TF1>("fit_function", plot.fit_function.c_str(), xMin, xMax);

        ratio->Fit(fct.get(), "MRNEQ");

        std::shared_ptr<TH1> errors = std::make_shared<TH1D>("errors", "errors", 100, xMin, xMax);
        errors->SetDirectory(nullptr);
        (TVirtualFitter::GetFitter())->GetConfidenceIntervals(errors.get(), 0.68);
        errors->SetStats(false);
        errors->SetMarkerSize(0);
        errors->SetFillColor(m_plotIt.getConfiguration().ratio_fit_error_fill_color);
        errors->SetFillStyle(m_plotIt.getConfiguration().ratio_fit_error_fill_style);
        errors->Draw("e3 same");

        fct->SetLineWidth(m_plotIt.getConfiguration().ratio_fit_line_width);
        fct->SetLineColor(m_plotIt.getConfiguration().ratio_fit_line_color);
        fct->SetLineStyle(m_plotIt.getConfiguration().ratio_fit_line_style);
        fct->Draw("same");

        if (plot.fit_legend.length() > 0) {
          uint32_t fit_parameters = fct->GetNpar();
          boost::format formatter = get_formatter(plot.fit_legend);

          for (uint32_t i = 0; i < fit_parameters; i++) {
            formatter % fct->GetParameter(i);
          }

          std::string legend = formatter.str();

          std::shared_ptr<TLatex> t(new TLatex(plot.fit_legend_position.x, plot.fit_legend_position.y, legend.c_str()));
          t->SetNDC(true);
          t->SetTextFont(43);
          t->SetTextSize(LABEL_FONTSIZE - 4);
          t->Draw();

          m_plotIt.addTemporaryObject(t);
        }

        m_plotIt.addTemporaryObject(errors);
        m_plotIt.addTemporaryObject(fct);
      }

      h_low_pad_axis->Draw("P E X0 same");
      ratio->Draw("P same");

      // Hide top pad label
      hideXTitle(toDraw[0].first);

      m_plotIt.addTemporaryObject(h_low_pad_axis);
      m_plotIt.addTemporaryObject(ratio);
      m_plotIt.addTemporaryObject(h_systematics);
      m_plotIt.addTemporaryObject(hi_pad);
      m_plotIt.addTemporaryObject(low_pad);
    }

    gPad->Modified();
    gPad->Update();
    gPad->RedrawAxis();

    if (hi_pad.get())
      hi_pad->cd();

    return true;
  
    return true;
  }

  void TH1Plotter::setHistogramStyle(const File& file) {
    TH1* h = dynamic_cast<TH1*>(file.object);

    std::shared_ptr<PlotStyle> style = m_plotIt.getPlotStyle(file);

    if (style->fill_color != -1)
      h->SetFillColor(style->fill_color);

    if (style->fill_type != -1)
      h->SetFillStyle(style->fill_type);

    if (style->line_color != -1)
      h->SetLineColor(style->line_color);

    if (style->line_width != -1)
      h->SetLineWidth(style->line_width);

    if (style->line_type != -1)
      h->SetLineStyle(style->line_type);

    if (style->marker_size != -1)
      h->SetMarkerSize(style->marker_size);

    if (style->marker_color != -1)
      h->SetMarkerColor(style->marker_color);

    if (style->marker_type != -1)
      h->SetMarkerStyle(style->marker_type);

    if (file.type == MC && style->line_color == -1 && style->fill_color != -1)
      h->SetLineColor(style->fill_color);
  }

  void TH1Plotter::addOverflow(TH1* h, Type type, const Plot& plot) {

    size_t first_bin = 1;
    size_t last_bin = h->GetNbinsX();

    if (plot.x_axis_range.size() == 2) {
      std::shared_ptr<TH1> copy(dynamic_cast<TH1*>(h->Clone()));
      copy->SetDirectory(nullptr);
      copy->GetXaxis()->SetRangeUser(plot.x_axis_range[0], plot.x_axis_range[1]);

      // Find first and last bin corresponding to the given range
      first_bin = copy->GetXaxis()->GetFirst();
      last_bin = copy->GetXaxis()->GetLast();
    }

    // GetBinError returns sqrt(SumW2) for a given bin
    // SetBinError updates SumW2 for a given bin with error*error

    float underflow = 0;
    float underflow_sumw2 = 0;
    for (size_t i = 0; i < first_bin; i++) {
      underflow += h->GetBinContent(i);
      underflow_sumw2 += (h->GetBinError(i) * h->GetBinError(i));
    }

    float overflow = 0;
    float overflow_sumw2 = 0;
    for (size_t i = last_bin + 1; i <= (size_t) h->GetNbinsX() + 1; i++) {
      overflow += h->GetBinContent(i);
      overflow_sumw2 += (h->GetBinError(i) * h->GetBinError(i));
    }

    float first_bin_content = h->GetBinContent(first_bin);
    float first_bin_sumw2 = h->GetBinError(first_bin) * h->GetBinError(first_bin);

    float last_bin_content = h->GetBinContent(last_bin);
    float last_bin_sumw2 = h->GetBinError(last_bin) * h->GetBinError(last_bin);

    h->SetBinContent(first_bin, first_bin_content + underflow);
    if (type != DATA)
        h->SetBinError(first_bin, sqrt(underflow_sumw2 + first_bin_sumw2));

    h->SetBinContent(last_bin, last_bin_content + overflow);
    if (type != DATA)
        h->SetBinError(last_bin, sqrt(overflow_sumw2 + last_bin_sumw2));
  }
}
