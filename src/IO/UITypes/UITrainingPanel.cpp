#include "UITrainingPanel.h"
#include "../../Constants.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <nlnx/nx.hpp>
#include <nlnx/bitmap.hpp>

namespace ms
{
    namespace
    {
        constexpr int skills[] = {0, 5001001, 5001002, 5001003};
        constexpr const char* skill_names[] = {"普通攻击", "闪·百裂拳", "半月踢", "双弹射击"};
        // Extend native edge pixels while preserving authored corners. The middle
        // sample deliberately excludes baked-in labels (GROUP / OK).
        Texture native_slice(nl::node source, const std::string& key, int width, int height,
                             int left, int right, int top, int bottom, int tile_x=-1, int tile_y=-1)
        {
            nl::bitmap bitmap = source;
            if (!bitmap || bitmap.width() <= left + right || bitmap.height() < top + bottom) return {};
            const auto* pixels = static_cast<const uint8_t*>(bitmap.data());
            if (!pixels) return {};
            std::vector<uint8_t> output(width * height * 4);
            auto sample = [](int value, int target, int original, int before, int after) {
                return value < before ? value : value >= target-after ? original-(target-value) : before;
            };
            for (int y=0; y<height; ++y)
                for (int x=0; x<width; ++x)
                {
                    int sx=sample(x,width,bitmap.width(),left,right);
                    int sy=sample(y,height,bitmap.height(),top,bottom);
                    if (x>=left && x<width-right && tile_x>=0) sx=tile_x+(x-left)%8;
                    if (y>=top && y<height-bottom) {
                        if (tile_y>=0) sy=tile_y+(y-top)%8;
                        else sy=top+(y-top)*(bitmap.height()-top-bottom)/(height-top-bottom);
                    }
                    std::memcpy(&output[(y*width+x)*4], &pixels[(sy*bitmap.width()+sx)*4], 4);
                }
            return Texture::from_pixels("training/native/"+key+"/"+std::to_string(width)+"x"+std::to_string(height),
                                        width,height,std::move(output),{});
        }
        class PanelButton : public Button
        {
        public:
            PanelButton(Point<int16_t> at, Point<int16_t> size, bool close=false) : size(size), close_(close)
            {
                position = at; active = true; pressed = false; state = NORMAL;
                const char* states[]={"normal","mouseOver","pressed","disabled"};
                auto source=nl::nx::ui["Basic.img"][close ? "BtClose" : "BtOK"];
                for (int i=0;i<4;++i)
                    art[i]=close ? Texture(source[states[i]]["0"])
                        : native_slice(source[states[i]]["0"],std::string("button/")+states[i],size.x(),size.y(),8,8,3,3);
            }
            void label(const std::string& value)
            {
                if (value != label_) { label_ = value; text = Text(Text::A12M, Text::CENTER, Color::Name::WHITE, value); }
            }
            void draw(Point<int16_t> parent) const override
            {
                auto at = parent + position;
                int image=state==DISABLED ? 3 : state==MOUSEOVER ? 1 : state==PRESSED ? 2 : 0;
                auto& texture=art[image];
                if (texture.is_valid()) texture.draw(at+texture.get_origin());
                else ColorBox(size.x(),size.y(),Color::Name::PRUSSIANBLUE,1).draw(at);
                if (!close_) text.draw(at + Point<int16_t>(size.x()/2, (size.y()-16)/2));
            }
            void update() override {}
            Rectangle<int16_t> bounds(Point<int16_t> parent) const override { return {parent + position, parent + position + size}; }
            int16_t width() const override { return size.x(); }
            Point<int16_t> origin() const override { return {}; }
            Cursor::State send_cursor(bool, Point<int16_t>) override { return Cursor::IDLE; }
        private:
            Point<int16_t> size;
            bool close_;
            std::array<Texture,4> art;
            std::string label_;
            Text text;
        };
        void label(Point<int16_t> at, const std::string& text, Color::Name color = Color::Name::PRUSSIANBLUE)
        { Text(Text::A12M, Text::LEFT, color, text).draw(at); }
    }

    UITrainingPanel::UITrainingPanel(TrainingAssistStatus& status, Sender sender)
        : status(status), sender(std::move(sender))
    {
        auto native=nl::nx::ui["UIWindow2.img"]["UserList"]["Group"];
        frame_=native_slice(native["backgrnd"],"frame",440,456,12,12,24,12);
        interior_=native_slice(native["backgrnd2"],"interior",428,428,6,6,8,6,100,100);
        for (int i=0;i<4;++i) checks_[i]=Texture(nl::nx::ui["Basic.img"]["CheckBox3"][i]);
        screen_width = Constants::Constants::get().get_viewwidth();
        screen_height = Constants::Constants::get().get_viewheight();
        layout(); update();
    }

    void UITrainingPanel::layout()
    {
        dimension = expanded_ ? Point<int16_t>(440, 456) : Point<int16_t>(132, 30);
        position = Point<int16_t>(std::max<int>(0, screen_width - dimension.x() - 16),
            std::max<int>(0, std::min<int>(56, screen_height - dimension.y() - 44)));
        buttons.clear();
        auto add = [&](int id, int x, int y, int w, int h) {
            buttons[id] = std::make_unique<PanelButton>(Point<int16_t>(x,y), Point<int16_t>(w,h), expanded_ && id==0);
        };
        if (!expanded_) { add(0,0,0,132,30); return; }
        add(0,416,5,16,16);
        for (int row=0; row<4; ++row) add(row+1,320,86+row*38,96,26);
        add(5,184,238,232,26);
        add(6,184,274,108,26); add(7,308,274,108,26);
        add(8,16,410,88,28); add(9,116,410,208,28); add(10,336,410,80,28);
    }

    void UITrainingPanel::toggle()
    {
        expanded_ = !expanded_; layout();
        if (expanded_) request("sync");
        update();
    }
    void UITrainingPanel::update_screen(int16_t w, int16_t h) { screen_width=w; screen_height=h; layout(); update(); }
    void UITrainingPanel::request(const std::string& command)
    {
        pending_ = true; sent = std::chrono::steady_clock::now();
        feedback = "等待服务端确认…"; sender("@assist " + command);
    }
    void UITrainingPanel::update()
    {
        auto now = std::chrono::steady_clock::now();
        if (seen_revision != status.revision) { seen_revision=status.revision; pending_=false; feedback.clear(); }
        if (pending_ && now-sent > std::chrono::seconds(3))
        { pending_=false; status.known=false; feedback="请求超时，请刷新状态"; }
        if (expanded_ && !pending_ && now >= next_poll)
        { next_poll=now+std::chrono::seconds(2); sender("@assist sync"); }
        auto set = [&](int id, const std::string& text, bool enabled=true) {
            auto& button = *static_cast<PanelButton*>(buttons.at(id).get());
            button.label(text); button.set_active(enabled);
            if (!enabled) button.set_state(Button::DISABLED);
            else if (button.get_state()==Button::DISABLED) button.set_state(Button::NORMAL);
        };
        set(0, expanded_ ? "收起 F8" : "辅助设置 · F8");
        if (!expanded_) return;
        bool enabled = status.fresh() && status.allowed && !pending_;
        for (int id=1; id<=4; ++id)
        {
            bool on = id==1 ? status.invincible : id==2 ? status.silence : id==3 ? status.vacuum : status.auto_skill>=0;
            set(id, !status.fresh() ? "未同步" : on ? "已开启 · 关闭" : "已关闭 · 开启", enabled);
        }
        if (status.auto_skill>=0)
            for (int i=0;i<4;++i) if (skills[i]==status.auto_skill) selected_skill=i;
        set(5, std::string(skill_names[selected_skill])+"  ›", enabled && status.auto_skill<0);
        set(6, "范围 "+std::to_string(status.radius)+"  ›", enabled);
        set(7, "数量 "+std::to_string(status.limit)+"  ›", enabled);
        set(8,"刷新状态"); set(9,"一键停止全部辅助"); set(10,"收起");
    }
    void UITrainingPanel::draw(float alpha) const
    {
        if (!expanded_) { UIElement::draw(alpha); return; }
        if (frame_.is_valid()) frame_.draw(position);
        if (interior_.is_valid()) interior_.draw(position+Point<int16_t>(6,22));
        else ColorBox(428,428,Color::Name::PORCELAIN,1).draw(position+Point<int16_t>(6,22));
        Text(Text::A12B,Text::CENTER,Color::Name::YELLOW,"训练辅助 · F8").draw(position+Point<int16_t>(220,6));
        label(position+Point<int16_t>(16,42), !status.fresh() ? "尚未取得服务端状态" : status.allowed ? "当前地图：允许训练辅助" : "当前地图：未开放训练辅助");
        label(position+Point<int16_t>(16,62),"开关以服务端确认为准；沉默影响整张地图。",Color::Name::DUSTYGRAY);
        const char* rows[]={"角色无敌（训练免伤）","怪物沉默（包括 Boss）","定点吸怪（普通怪物）","原地在线挂机"};
        const bool checked[]={status.invincible,status.silence,status.vacuum,status.auto_skill>=0};
        for(int i=0;i<4;++i) {
            auto& icon=checks_[!status.fresh() || !status.allowed ? 2 : checked[i] ? 1 : 0];
            icon.draw(position+Point<int16_t>(16,94+i*38)+icon.get_origin());
            label(position+Point<int16_t>(38,90+i*38),rows[i]);
        }
        label(position+Point<int16_t>(16,242),"挂机技能");
        label(position+Point<int16_t>(16,278),"吸怪设置");
        label(position+Point<int16_t>(16,316),"快速战斗 / 无延迟");
        label(position+Point<int16_t>(320,316),"待实现",Color::Name::DUSTYGRAY);
        label(position+Point<int16_t>(16,344),!feedback.empty() ? feedback : !status.stop_reason.empty()
            ? status.stop_reason : "开启时固定吸怪点；重新开启可更换位置。");
        label(position+Point<int16_t>(16,368),"展开面板时挂机暂停，收起后继续。",Color::Name::DUSTYGRAY);
        label(position+Point<int16_t>(16,388),"手动游戏操作可停止挂机；设置不自动开启。",Color::Name::DUSTYGRAY);
        UIElement::draw(alpha);
    }
    Button::State UITrainingPanel::button_pressed(uint16_t id)
    {
        if (id==0 || id==10) { toggle(); return Button::NORMAL; }
        if (id==8) request("sync");
        else if (id==9) request("off");
        else if (!pending_ && status.fresh() && status.allowed)
        {
            if (id==1) request(std::string("invincible ")+(status.invincible ? "off":"on"));
            if (id==2) request(std::string("silence ")+(status.silence ? "off":"on"));
            if (id==3) request(std::string("vac ")+(status.vacuum ? "off":"on"));
            if (id==4) request(status.auto_skill>=0 ? "auto off" : "auto on "+std::to_string(skills[selected_skill]));
            if (id==5 && status.auto_skill<0) selected_skill=(selected_skill+1)%4;
            if (id==6) request("vac settings "+std::to_string(status.radius<400 ? 400 : status.radius<800 ? 800 : 200)+" "+std::to_string(status.limit));
            if (id==7) request("vac settings "+std::to_string(status.radius)+" "+std::to_string(status.limit<5 ? 5 : status.limit<10 ? 10 : status.limit<20 ? 20 : 5));
        }
        // Keep button objects alive until the cursor dispatcher has returned.
        return Button::NORMAL;
    }
    Cursor::State UITrainingPanel::send_cursor(bool down, Point<int16_t> at)
    {
        if (down && held_) return Cursor::IDLE;
        held_=down;
        // layout() can replace buttons on expand/collapse; do not call base dispatcher
        // for those controls because it keeps a pointer to the clicked button.
        for (auto id : {0,10})
            if (buttons.count(id) && buttons.at(id)->bounds(position).contains(at) && down)
            { toggle(); return Cursor::IDLE; }
        return UIElement::send_cursor(down,at);
    }
    void UITrainingPanel::remove_cursor() { held_=false; UIElement::remove_cursor(); }
}
