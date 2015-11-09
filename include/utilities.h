#pragma once

#include <defines.h>

#include <plotIt.h>

#include <boost/format.hpp>

namespace plotIt {
  TStyle* createStyle();

  boost::format get_formatter(const std::string format_string);

  #define CAST_AND_CALL(OBJECT, FUNCTION, ...) \
      if (dynamic_cast<TH1*>(OBJECT)) \
        FUNCTION(dynamic_cast<TH1*>(OBJECT), ##__VA_ARGS__); \
      else if (dynamic_cast<THStack*>(OBJECT)) \
        FUNCTION(dynamic_cast<THStack*>(OBJECT), ##__VA_ARGS__);

  #define CAST_AND_RETURN(OBJECT, FUNCTION, ...) \
      if (dynamic_cast<TH1*>(OBJECT)) \
        return FUNCTION(dynamic_cast<TH1*>(OBJECT), ##__VA_ARGS__); \
      else if (dynamic_cast<THStack*>(OBJECT)) \
        return FUNCTION(dynamic_cast<THStack*>(OBJECT), ##__VA_ARGS__);

  #define CAST_TO_HIST_AND_CALL(OBJECT, FUNCTION, ...) \
      if (dynamic_cast<TH1*>(OBJECT)) \
        FUNCTION(dynamic_cast<TH1*>(OBJECT), ##__VA_ARGS__); \
      else if (dynamic_cast<THStack*>(OBJECT)) \
        FUNCTION(dynamic_cast<THStack*>(OBJECT)->GetHistogram(), ##__VA_ARGS__);

  template<class T>
    void setAxisTitles(T* object, Plot& plot) {
      if (plot.x_axis.length() > 0 && object->GetXaxis()) {
        object->GetXaxis()->SetTitle(plot.x_axis.c_str());
      }

      if (plot.y_axis.length() > 0 && object->GetYaxis()) {
        float binSize = object->GetXaxis()->GetBinWidth(1);
        std::string title = plot.y_axis;

        boost::format formatter = get_formatter(plot.y_axis_format);
        object->GetYaxis()->SetTitle((formatter % title % binSize).str().c_str());
      }

      if (plot.show_ratio && object->GetXaxis())
        object->GetXaxis()->SetLabelSize(0);
    }

  void setAxisTitles(TObject* object, Plot& plot);

  template<class T>
    void setDefaultStyle(T* object, float topBottomScaleFactor) {

      object->SetLabelFont(43, "XYZ");
      object->SetTitleFont(43, "XYZ");
      object->SetLabelSize(LABEL_FONTSIZE, "XYZ");
      object->SetTitleSize(TITLE_FONTSIZE, "XYZ");

      object->GetYaxis()->SetTitle("Data / MC");
      object->GetYaxis()->SetNdivisions(510);
      object->GetYaxis()->SetTitleOffset(2.5);
      object->GetYaxis()->SetLabelOffset(0.01);
      object->GetYaxis()->SetTickLength(0.03);

      object->GetXaxis()->SetTitleOffset(1.5 * topBottomScaleFactor);
      object->GetXaxis()->SetLabelOffset(0.012 * topBottomScaleFactor);
      object->GetXaxis()->SetTickLength(0.03);
      
    }

  void setDefaultStyle(TObject* object, float topBottomScaleFactor);


  template<class T>
    void hideXTitle(T* object) {
      object->GetXaxis()->SetTitle("");
      object->GetXaxis()->SetTitleSize();
    }

  void hideXTitle(TObject* object);

  template<class T>
    float getMaximum(T* object) {
      return object->GetMaximum();
    }

  float getMaximum(TObject* object);

  template<class T>
    float getMinimum(T* object) {
      return object->GetMinimum();
    }

  float getMinimum(TObject* object);

  template<class T>
    void setMaximum(T* object, float minimum) {
      object->SetMaximum(minimum);
    }

  void setMaximum(TObject* object, float minimum);

  template<class T>
    void setMinimum(T* object, float minimum) {
      object->SetMinimum(minimum);
    }

  void setMinimum(TObject* object, float minimum);

  template<class T>
    void setRange(T* object, Plot& plot, bool onlyX = false) {
      if (plot.x_axis_range.size() == 2)
        object->GetXaxis()->SetRangeUser(plot.x_axis_range[0], plot.x_axis_range[1]);
      if (!onlyX && plot.y_axis_range.size() == 2) {
        object->SetMinimum(plot.y_axis_range[0]);
        object->SetMaximum(plot.y_axis_range[1]);
      }
    }

  void setRange(TObject* object, Plot& plot, bool onlyX = false);

  template<class T> void removeNegativeBins(T* object) {
      for (size_t i = 0; i <= (size_t) object->GetNbinsX() + 1; i++) {
          float content = object->GetBinContent(i);
          if (content < 0) {
            object->SetBinContent(i, 0);
            if (object->GetSumw2N() > 0)
                object->SetBinError(i, 0);
          }
      }
  }
}
