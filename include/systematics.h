#pragma once

#include <string>
#include <memory>
#include <regex>

namespace YAML {
    class Node;
}

class TH1;
class TObject;

namespace plotIt {

    enum Variation {
        NOMINAL = 0,
        UP = +1,
        DOWN = -1
    };

    struct Plot;
    struct File;
    struct Systematic;

    struct SystematicSet {
        std::shared_ptr<TObject> true_nominal_shape;
        std::shared_ptr<TObject> true_up_shape;
        std::shared_ptr<TObject> true_down_shape;

        std::shared_ptr<TObject> nominal_shape;
        std::shared_ptr<TObject> up_shape;
        std::shared_ptr<TObject> down_shape;

        void update();

        /**
         * Assume objects are histograms and scale them by the specified factor
         **/
        void scale(float factor);

        /**
         * Assume objects are histograms and rebin them by the specified factor
         **/
        void rebin(size_t factor);

        std::string name() const;
        std::string prettyName() const;

        private:
        friend struct Systematic;

        SystematicSet(Systematic&);
        Systematic* parent;
    };

    struct Systematic {

        std::string name;
        std::string pretty_name;
        std::regex on;

        /**
         * Apply the systematic on the given set
         **/
        virtual void apply(SystematicSet&);

        /**
         * Load from the file the necessary objects. Default implementation only
         * clones the nominal histograms. Up and down variation are computed when
         * apply is called.
         */
        virtual SystematicSet newSet(TObject* nominal, File& file, const Plot& plot);
    };

    struct ConstantSystematic: public Systematic {
        ConstantSystematic(const YAML::Node& node);

        virtual void apply(SystematicSet&) override;

        float value;
    };

    struct LogNormalSystematic: public Systematic {
        LogNormalSystematic(const YAML::Node& node);

        virtual void apply(SystematicSet&) override;

        void eval();

        float prior = 0;
        float postfit = 0;
        float postfit_error_up = 1;
        float postfit_error_down = 1;

        float value = 0;
        float value_up = 0;
        float value_down = 0;
    };

    struct ShapeSystematic: public Systematic {
        ShapeSystematic(const YAML::Node& node);
        virtual SystematicSet newSet(TObject* nominal, File& file, const Plot& plot) override;
    };

    class SystematicFactory {
        public:
            static std::shared_ptr<Systematic> create(const std::string& name, const std::string& type, const YAML::Node& node);
    };

    using SystematicPtr = std::shared_ptr<Systematic>;
}
