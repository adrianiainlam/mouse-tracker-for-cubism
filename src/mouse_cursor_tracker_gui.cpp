#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <mutex>

#include <gtkmm/application.h>
#include <gtkmm/builder.h>
#include <gtkmm/window.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/button.h>
#include <gtkmm/scale.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/expander.h>

#include "mouse_cursor_tracker.h"

extern std::vector<std::string> live2dParams;
extern std::vector<std::pair<std::string, int> > MCT_motions;
extern std::vector<std::string> MCT_expressions;
extern std::map<std::string, double> *MCT_overrideMap;

#define GET_WIDGET_ASSERT(id, widgetPtr) do { \
        m_builder->get_widget(id, widgetPtr); \
        if (!widgetPtr) abort();              \
    } while (0)

void MouseCursorTracker::onMotionStartButton(void)
{
    std::unique_lock<std::mutex> lock(m_motionMutex, std::defer_lock);
    lock.lock();

    Gtk::ComboBoxText *cbt;
    GET_WIDGET_ASSERT("comboBoxMotions", cbt);
    std::string motionStr = cbt->get_active_text();
    if (!motionStr.empty())
    {
        std::stringstream ss(motionStr);
        ss >> m_motionGroup >> m_motionNumber;
    }

    GET_WIDGET_ASSERT("comboBoxMotionPriority", cbt);
    std::string priority = cbt->get_active_id();
    if (priority == "priorityForce")
    {
        m_motionPriority = MotionPriority::force;
    }
    else if (priority == "priorityNormal")
    {
        m_motionPriority = MotionPriority::normal;
    }
    else if (priority == "priorityIdle")
    {
        m_motionPriority = MotionPriority::idle;
    }
    else
    {
        m_motionPriority = MotionPriority::none;
    }

    lock.unlock();
}

void MouseCursorTracker::onExpressionStartButton(void)
{
    std::unique_lock<std::mutex> lock(m_motionMutex, std::defer_lock);
    lock.lock();

    Gtk::ComboBoxText *cbt;
    GET_WIDGET_ASSERT("comboBoxExpressions", cbt);
    std::string expStr = cbt->get_active_text();
    if (!expStr.empty())
    {
        m_expression = expStr;
    }

    lock.unlock();
}

void MouseCursorTracker::onParamUpdateButton(Gtk::Scale *scale, bool isInc)
{
    Glib::RefPtr<Gtk::Adjustment> adj = scale->get_adjustment();
    double value = adj->get_value();
    double increment = adj->get_step_increment();
    adj->set_value(value + increment * (isInc ? 1 : -1));
}

void MouseCursorTracker::onParamValChanged(Glib::RefPtr<Gtk::Adjustment> adj, std::string paramName)
{
    bool isResetting = std::find(
        m_onClearResettingParams.begin(),
        m_onClearResettingParams.end(),
        paramName) != m_onClearResettingParams.end();

    if (isResetting)
    {
        // Value changed event caused by onClearButton,
        // don't change underlying value
        return;
    }

    double value = adj->get_value();
    m_overrideMap["Param" + paramName] = value;
    /* Use get_object instead of get_widget to avoid
     * "widget not found" error messages */
    auto obj = m_builder->get_object("button" + paramName + "Clear");
    if (obj)
    {
        auto button = Glib::RefPtr<Gtk::Button>::cast_dynamic(obj);
        button->set_visible(true);
    }
}

void MouseCursorTracker::onClearButton(Glib::RefPtr<Gtk::Button> button, std::string paramName, Glib::RefPtr<Gtk::Adjustment> adj)
{
    m_overrideMap.erase("Param" + paramName);

    // Reset the GtkScale display value to 0, without running the callback.
    m_onClearResettingParams.push_back(paramName);
    if (adj)
    {
        double value = 0;
        // Special case for MouthForm: use value from config file
        if (paramName == "MouthForm")
        {
            value = m_cfg.mouthForm;
        }
        adj->set_value(value);
    }
    m_onClearResettingParams.erase(
        std::remove(
            m_onClearResettingParams.begin(),
            m_onClearResettingParams.end(),
            paramName),
        m_onClearResettingParams.end()
    );

    button->set_visible(false);
}

void MouseCursorTracker::onAutoToggle(Gtk::CheckButton *check, Gtk::Scale *scale,
                                      Gtk::Button *buttonDec, Gtk::Button *buttonInc,
                                      std::string paramName)
{
    bool active = check->get_active();
    scale->set_sensitive(!active);
    buttonDec->set_sensitive(!active);
    buttonInc->set_sensitive(!active);

    if (active)
    {
        m_overrideMap.erase(paramName);
    }
    else
    {
        auto adj = scale->get_adjustment();
        double value = adj->get_value();
        m_overrideMap[paramName] = value;
    }

    // Special cases for lip sync and auto breath flags
    if (paramName == "ParamMouthOpenY")
    {
        m_cfg.useLipSync = active;
    }
    else if (paramName == "ParamBreath")
    {
        m_cfg.autoBreath = active;
    }
}

void MouseCursorTracker::onExpanderChange(Gtk::Window *window)
{
    // Shrink window if enlarged by GtkExpander
    window->resize(1, 1);
}

void MouseCursorTracker::guiLoop(void)
{
    m_builder = Gtk::Builder::create_from_file("gui.glade");

    // Add motions list to combobox
    Gtk::ComboBoxText *motionsCbt;
    GET_WIDGET_ASSERT("comboBoxMotions", motionsCbt);
    for (auto it = MCT_motions.begin(); it != MCT_motions.end(); ++it)
    {
        for (int i = 0; i < it->second; i++)
        {
            std::stringstream ss;
            ss << it->first << " " << i;
            motionsCbt->append(ss.str());
        }
    }

    // Add expressions list to combobox
    Gtk::ComboBoxText *expsCbt;
    GET_WIDGET_ASSERT("comboBoxExpressions", expsCbt);
    for (auto it = MCT_expressions.begin(); it != MCT_expressions.end(); ++it)
    {
        expsCbt->append(*it);
    }


    // Add scale tick marks
    Gtk::Scale *scale;

    std::vector<std::string> ticksAtZero =
    {
        "scaleAngleX", "scaleAngleY", "scaleAngleZ",
        "scaleEyeBallX", "scaleEyeBallY", "scaleEyeBallForm",
        "scaleMouthForm",
        "scaleBrowLX", "scaleBrowLY", "scaleBrowLAngle",
        "scaleBrowLForm",
        "scaleBrowRX", "scaleBrowRY", "scaleBrowRAngle",
        "scaleBrowRForm",
        "scaleHairFront", "scaleHairSide", "scaleHairBack",
        "scaleBodyAngleX", "scaleBodyAngleY", "scaleBodyAngleZ",
        "scaleBustX", "scaleBustY", "scaleBaseX", "scaleBaseY",
        "scaleArmLA", "scaleArmLB", "scaleArmRA", "scaleArmRB",
        "scaleHandL", "scaleHandR"
    };

    for (auto it = ticksAtZero.begin(); it != ticksAtZero.end(); ++it)
    {
        GET_WIDGET_ASSERT(*it, scale);
        scale->add_mark(0, Gtk::PositionType::POS_BOTTOM, "");
    }

    GET_WIDGET_ASSERT("scaleEyeLOpen", scale);
    scale->add_mark(0, Gtk::PositionType::POS_BOTTOM, "");
    scale->add_mark(1, Gtk::PositionType::POS_BOTTOM, "");
    GET_WIDGET_ASSERT("scaleEyeROpen", scale);
    scale->add_mark(0, Gtk::PositionType::POS_BOTTOM, "");
    scale->add_mark(1, Gtk::PositionType::POS_BOTTOM, "");

    GET_WIDGET_ASSERT("scaleMouthOpenY", scale);
    scale->add_mark(1, Gtk::PositionType::POS_BOTTOM, "");

    // Bind button handlers
    Gtk::Button *button;
    GET_WIDGET_ASSERT("buttonStartMotion", button);
    button->signal_clicked().connect(sigc::mem_fun(*this, &MouseCursorTracker::onMotionStartButton));

    GET_WIDGET_ASSERT("buttonStartExpression", button);
    button->signal_clicked().connect(sigc::mem_fun(*this, &MouseCursorTracker::onExpressionStartButton));

    // Bind button handlers for increment / decrement buttons
    for (auto it = live2dParams.begin(); it != live2dParams.end(); ++it)
    {
        std::string paramName = *it; // e.g. ParamAngleX
        paramName.erase(0, 5);       // e.g. AngleX
        std::string buttonDecId = "button" + paramName + "Dec";
        std::string buttonIncId = "button" + paramName + "Inc";
        std::string buttonClrId = "button" + paramName + "Clear";
        std::string scaleId     = "scale" + paramName;

        m_builder->get_widget(scaleId, scale);

        m_builder->get_widget(buttonDecId, button);
        if (button && scale)
        {
            button->signal_clicked().connect(
                sigc::bind<Gtk::Scale *, bool>(
                    sigc::mem_fun(*this, &MouseCursorTracker::onParamUpdateButton),
                    scale, false
                )
            );
        }

        m_builder->get_widget(buttonIncId, button);
        if (button && scale)
        {
            button->signal_clicked().connect(
                sigc::bind<Gtk::Scale *, bool>(
                    sigc::mem_fun(*this, &MouseCursorTracker::onParamUpdateButton),
                    scale, true
                )
            );
        }

        Glib::RefPtr<Gtk::Adjustment> adj;
        if (scale)
        {
            adj = scale->get_adjustment();
            if (!adj) abort();
            adj->signal_value_changed().connect(
                sigc::bind<Glib::RefPtr<Gtk::Adjustment>, std::string>(
                    sigc::mem_fun(*this, &MouseCursorTracker::onParamValChanged),
                    adj, paramName
                )
            );
        }

        /* Use get_object instead of get_widget to avoid
         * "widget not found" error messages */
        auto obj = m_builder->get_object(buttonClrId);
        if (obj)
        {
            auto buttonClr = Glib::RefPtr<Gtk::Button>::cast_dynamic(obj);
            buttonClr->signal_clicked().connect(
                sigc::bind<Glib::RefPtr<Gtk::Button>, std::string, Glib::RefPtr<Gtk::Adjustment> >(
                    sigc::mem_fun(*this, &MouseCursorTracker::onClearButton),
                    buttonClr, paramName, adj
                )
            );
        }
    }

    // Bind handlers for auto params check boxes
    Gtk::CheckButton *check;
    Gtk::Button *buttonInc;
    Gtk::Button *buttonDec;

    std::vector<std::string> autoTracked =
    {
        "AngleX", "AngleY", "EyeLOpen", "EyeROpen", "MouthOpenY", "Breath"
    };

    for (auto it = autoTracked.begin(); it != autoTracked.end(); ++it)
    {
        GET_WIDGET_ASSERT("check" + *it, check);
        GET_WIDGET_ASSERT("scale" + *it, scale);
        GET_WIDGET_ASSERT("button" + *it + "Inc", buttonInc);
        GET_WIDGET_ASSERT("button" + *it + "Dec", buttonDec);
        check->signal_toggled().connect(
            sigc::bind<Gtk::CheckButton *, Gtk::Scale *, Gtk::Button *, Gtk::Button *, std::string>(
                sigc::mem_fun(*this, &MouseCursorTracker::onAutoToggle),
                check, scale, buttonDec, buttonInc, "Param" + *it
            )
        );
    }

    // Set some values from config file
    GET_WIDGET_ASSERT("checkMouthOpenY", check);
    check->set_active(m_cfg.useLipSync);
    GET_WIDGET_ASSERT("checkBreath", check);
    check->set_active(m_cfg.autoBreath);
    GET_WIDGET_ASSERT("scaleMouthForm", scale);
    auto adj = scale->get_adjustment();
    // Don't trigger value changed event
    m_onClearResettingParams.push_back("MouthForm");
    adj->set_value(m_cfg.mouthForm);
    m_onClearResettingParams.erase(
        std::remove(
            m_onClearResettingParams.begin(),
            m_onClearResettingParams.end(),
            "MouthForm"),
        m_onClearResettingParams.end()
    );

    Gtk::Window *window;
    GET_WIDGET_ASSERT("windowMain", window);

    std::vector<std::string> expanders =
    {
        "expanderHead", "expanderEyes", "expanderEyebrows",
        "expanderMouthFace", "expanderHair", "expanderBody",
        "expanderArmsHands"
    };
    Gtk::Expander *expander;
    for (auto it = expanders.begin(); it != expanders.end(); ++it)
    {
        m_builder->get_widget(*it, expander);
        if (expander)
        {
            expander->property_expanded().signal_changed().connect(
                sigc::bind<Gtk::Window *>(
                    sigc::mem_fun(*this, &MouseCursorTracker::onExpanderChange),
                    window
                )
            );
        }
    }

    m_gtkapp->run(*window);
}

