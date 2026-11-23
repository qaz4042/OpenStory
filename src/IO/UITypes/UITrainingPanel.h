#pragma once
#include "../UIElement.h"
#include "../../Gameplay/TrainingAssistStatus.h"
#include <functional>
#include <array>
#include <vector>

namespace ms
{
    class UITrainingPanel : public UIElement
    {
    public:
        static constexpr Type TYPE = UIElement::TRAININGASSIST;
        static constexpr bool FOCUSED = false, TOGGLED = false;
        using Sender = std::function<void(const std::string&)>;
        UITrainingPanel(TrainingAssistStatus& status, Sender sender);
        void draw(float alpha) const override;
        void update() override;
        void update_screen(int16_t width, int16_t height) override;
        Cursor::State send_cursor(bool down, Point<int16_t> at) override;
        void remove_cursor() override;
        Type get_type() const override { return TYPE; }
        void toggle();
        void job_changed();
        bool expanded() const { return expanded_; }
    protected:
        Button::State button_pressed(uint16_t id) override;
    private:
        void layout();
        void refresh_skills();
        void start_rotation();
        std::string skill_label(int id) const;
        std::vector<int> available_skills{0};
        std::array<int,4> rotation_skills{{0,-1,-1,-1}};
        std::array<int,4> rotation_ms{{1000,60000,60000,60000}};
        bool rotation_view=false, use_rotation=false;
        void request(const std::string& command);
        Texture frame_, interior_;
        std::array<Texture, 4> checks_;
        TrainingAssistStatus& status;
        Sender sender;
        bool expanded_ = false, held_ = false, pending_ = false;
        unsigned seen_revision = 0;
        int selected_skill = 0;
        int16_t screen_width = 800, screen_height = 600;
        std::string feedback;
        std::chrono::steady_clock::time_point sent{}, next_poll{};
    };
}
