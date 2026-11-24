#include "UITrainingPanel.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Data/SkillData.h"
#include "../../Character/Player.h"
#include "../../Util/Misc.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <nlnx/nx.hpp>
#include <nlnx/bitmap.hpp>

namespace ms
{
    namespace
    {
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
            PanelButton(Point<int16_t> at, Point<int16_t> size, bool close=false, bool check=false) : size(size), close_(close), check_(check)
            {
                position = at; active = true; pressed = false; state = NORMAL;
                if (check_) {
                    for (int i=0;i<4;++i) art[i]=Texture(nl::nx::ui["Basic.img"]["CheckBox3"][i]);
                    return;
                }
                const char* states[]={"normal","mouseOver","pressed","disabled"};
                auto source=nl::nx::ui["Basic.img"][close ? "BtClose" : "BtOK"];
                for (int i=0;i<4;++i)
                    art[i]=close ? Texture(source[states[i]]["0"])
                        : native_slice(source[states[i]]["0"],std::string("button/")+states[i],size.x(),size.y(),8,8,3,3);
            }
            void checked(bool value) { checked_=value; }
            void label(const std::string& value)
            {
                if (value != label_) { label_ = value; text = Text(Text::A12M, check_ ? Text::LEFT : Text::CENTER, check_ ? Color::Name::PRUSSIANBLUE : Color::Name::WHITE, value); }
            }
            void draw(Point<int16_t> parent) const override
            {
                auto at = parent + position;
                if (check_) {
                    auto& icon=art[state==DISABLED ? (checked_ ? 3 : 2) : checked_ ? 1 : 0];
                    icon.draw(at+Point<int16_t>(0,5)+icon.get_origin());
                    text.draw(at+Point<int16_t>(22,3));
                    return;
                }
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
            bool close_, check_, checked_=false;
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
        frame_=native_slice(native["backgrnd"],"frame",300,390,12,12,24,12);
        interior_=native_slice(native["backgrnd2"],"interior",288,362,6,6,8,6,100,100);
        for (int i=0;i<4;++i) checks_[i]=Texture(nl::nx::ui["Basic.img"]["CheckBox3"][i]);
        screen_width = Constants::Constants::get().get_viewwidth();
        screen_height = Constants::Constants::get().get_viewheight();
        refresh_skills(); layout(); update();
    }
    void UITrainingPanel::job_changed() {
        rotation_skills = {{0, -1, -1, -1}};
        selected_skill = 0;
        use_rotation = false;
        refresh_skills();
        feedback = "职业已变更，请重新选择挂机技能";
    }
    void UITrainingPanel::refresh_skills() {
        available_skills={0};
        for (const auto& item : Stage::get().get_player().get_skills().get_entries())
            if (item.second.level>0 && TrainingAutoAttack::supported(item.first)) available_skills.push_back(item.first);
        std::sort(available_skills.begin()+1,available_skills.end());
        if (selected_skill>=static_cast<int>(available_skills.size())) selected_skill=0;
    }
    std::string UITrainingPanel::skill_label(int id) const {
        if (id<0) return "不使用";
        if (id==0) return "普通攻击";
        if (id==5001001) return "攻击·闪·百裂拳";
        if (id==5001002) return "攻击·半月踢";
        if (id==5001003) return "攻击·双弹射击";
        if (id==5001005) return "增益·冲锋";
        const auto& name=SkillData::get(id).get_name();
        return (training_buff(id) ? "增益·" : "攻击·")+(name.empty() ? std::to_string(id) : name);
    }
    void UITrainingPanel::start_rotation() {
        std::string command="rotation"; bool any=false;
        std::set<int> seen;
        for (int i=0;i<4;++i) if (rotation_skills[i]>=0) {
            if (!seen.insert(rotation_skills[i]).second) { feedback="同一技能不能重复，请更换"; return; }
            any=true; command+=" "+std::to_string(rotation_skills[i])+":"+std::to_string(rotation_ms[i]);
        }
        if (any) { use_rotation=true; request(command); } else feedback="请至少设置一个技能";
    }

    void UITrainingPanel::layout()
    {
        const Point<int16_t> collapsed_size(104, 24);
        dimension = expanded_ ? Point<int16_t>(300, 390) : collapsed_size;
        position = Point<int16_t>(std::max<int>(0, screen_width - dimension.x() - 16),
            std::max<int>(0, std::min<int>(56, screen_height - dimension.y() - 44)));
        buttons.clear();
        auto add = [&](int id, int x, int y, int w, int h) {
            buttons[id] = std::make_unique<PanelButton>(Point<int16_t>(x,y), Point<int16_t>(w,h), expanded_ && id==0, (id>=1 && id<=4) || id==6 || id==7 || id==12 || id==14);
        };
        if (!expanded_) { add(0,0,0,collapsed_size.x(),collapsed_size.y()); return; }
        add(0,276,5,16,16);
        if (rotation_view) {
            for (int row=0;row<4;++row) {
                add(20+row,16,66+row*44,174,28); add(30+row,198,66+row*44,86,28);
            }
            add(40,16,270,80,28); add(41,108,270,176,28); return;
        }
        for (int row=0; row<4; ++row) add(row+1,16,52+row*26,268,24);
        add(5,88,158,152,24); add(13,244,158,40,24);
        add(6,16,186,128,24); add(7,156,186,128,24);
        add(12,16,234,128,24); add(14,156,234,128,24);
        add(15,16,260,268,24);
        add(8,16,354,68,24); add(9,94,354,112,24); add(10,216,354,68,24);
    }

    void UITrainingPanel::toggle()
    {
        expanded_ = !expanded_; layout();
        if (expanded_) { refresh_skills(); request("sync"); }
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
        set(0, expanded_ ? "收起 F8" : status.vacuum_pinned >= 0
            ? "已吸 " + std::to_string(status.vacuum_pinned) + " 只 · F8" : "辅助设置 · F8");
        if (!expanded_) return;
        bool enabled = status.fresh() && status.allowed && !pending_;
        if (rotation_view) {
            for (int row=0;row<4;++row) {
                set(20+row,skill_label(rotation_skills[row]),!pending_);
                std::string seconds=std::to_string(rotation_ms[row]/1000.0);
                seconds.erase(seconds.find_last_not_of('0')+1); if (seconds.back()=='.') seconds.pop_back();
                set(30+row,seconds+"秒 ›",!pending_ && rotation_skills[row]>=0);
            }
            set(40,"返回"); set(41,status.auto_skill>=0 ? "更新轮换并继续" : "应用并开始",enabled); return;
        }
        for (int id=1; id<=4; ++id)
        {
            bool on = id==1 ? status.invincible : id==2 ? status.silence : id==3 ? status.vacuum : status.auto_skill>=0;
            static const char* names[]={"角色无敌", "怪物沉默（含 Boss）", "全图吸怪", "自动挂机"};
            set(id, names[id-1], enabled);
            static_cast<PanelButton*>(buttons.at(id).get())->checked(status.fresh() && status.allowed && on);
        }
        if (status.auto_skill>=0)
            for (size_t i=0;i<available_skills.size();++i) if (available_skills[i]==status.auto_skill) selected_skill=i;
        set(5,use_rotation ? "已配置技能轮换" : skill_label(available_skills[selected_skill])+" ›",enabled && status.auto_skill<0);
        set(13,"轮换",!pending_);
        int flight = Stage::get().get_flight_mode();
        set(6,"虚拟飞 1–5×"); set(7,"真实飞 2×");
        static_cast<PanelButton*>(buttons.at(6).get())->checked(flight==1);
        static_cast<PanelButton*>(buttons.at(7).get())->checked(flight==2);
        set(12,"宠吸·金币", enabled && status.pet_vacuum_split_known);
        set(14,"宠吸·物品", enabled && status.pet_vacuum_split_known);
        static_cast<PanelButton*>(buttons.at(12).get())->checked(status.fresh() && status.pet_vacuum_mesos);
        static_cast<PanelButton*>(buttons.at(14).get())->checked(status.fresh() && status.pet_vacuum_items);
        int rapid_skill = status.rapid_rate ? status.rapid_skill : available_skills[selected_skill];
        set(15,!status.rapid_known ? "高频连发 · 需配套服务端" : status.rapid_rate
            ? "高频连发 · "+std::to_string(status.rapid_rate)+"次/秒 ›" : "高频连发 · 关闭 ›",
            enabled && status.rapid_known && TrainingAutoAttack::rapid_supported(rapid_skill));
        set(8,"刷新"); set(9,"全部停止"); set(10,"收起");
    }
    void UITrainingPanel::draw(float alpha) const
    {
        if (!expanded_) { UIElement::draw(alpha); return; }
        if (frame_.is_valid()) frame_.draw(position);
        if (interior_.is_valid()) interior_.draw(position+Point<int16_t>(6,22));
        else ColorBox(288,362,Color::Name::PORCELAIN,1).draw(position+Point<int16_t>(6,22));
        Text(Text::A12B,Text::CENTER,Color::Name::YELLOW,"辅助 · F8").draw(position+Point<int16_t>(150,6));
        label(position+Point<int16_t>(16,32), !feedback.empty() ? feedback : !status.fresh() ? "未同步 · 请刷新" : status.allowed ? "已同步" : "当前地图未开放");
        if (rotation_view) {
            label(position+Point<int16_t>(16,48),"点击技能切换 · 右侧设置重复间隔");
            for (int row=0;row<4;++row) if (rotation_skills[row]>0) {
                int id=rotation_skills[row];
                auto level=Stage::get().get_player().get_skilllevel(id);
                auto source=nl::nx::skill[string_format::extend_id(id/10000,3)+".img"]["skill"][string_format::extend_id(id,7)]["level"][std::to_string(level)];
                label(position+Point<int16_t>(18,96+row*44),"冷却 "+std::to_string(source["cooltime"].get_integer())+"秒 · MP "+std::to_string(source["mpCon"].get_integer()),Color::Name::DUSTYGRAY);
            }
            label(position+Point<int16_t>(16,242),"间隔可调；动作/冷却中等待，不连发");
            UIElement::draw(alpha); return;
        }
        label(position+Point<int16_t>(16,162),"挂机技能");
        label(position+Point<int16_t>(16,214),status.vacuum_pinned < 0 ? "吸怪数量待同步"
            : "吸怪 " + std::to_string(status.vacuum_pinned) + " / " + std::to_string(status.vacuum_eligible) + "（不含 Boss）");

        auto rate_text = [&](unsigned long long count) {
            auto tenths = status.rapid_elapsed ? count*10000/status.rapid_elapsed : 0;
            return std::to_string(tenths/10)+"."+std::to_string(tenths%10);
        };
        label(position+Point<int16_t>(16,286),status.rapid_rate
            ? "服务端 "+rate_text(status.rapid_accepted)+"次/秒 · 有效 "+rate_text(status.rapid_lines)+"段/秒"
            : "实验档 10 / 20 / 40次/秒 · 海盗一转");
        label(position+Point<int16_t>(16,302),status.rapid_rate
            ? "接收 "+std::to_string(status.rapid_accepted)+" · 限流 "+std::to_string(status.rapid_rejected)
            : "双弹每次2段；逐次消耗MP和弹药",Color::Name::DUSTYGRAY);
        label(position+Point<int16_t>(16,318),status.rapid_rate
            ? "累计扣血 "+std::to_string(status.rapid_damage)+" · 多目标合计"
            : "技能轮换独立使用；切图停止高频",Color::Name::DUSTYGRAY);
        label(position+Point<int16_t>(16,334),status.stop_reason.empty() ? "展开暂停飞行 · 全部停止可关闭连发" : status.stop_reason,Color::Name::DUSTYGRAY);
        UIElement::draw(alpha);
    }
    Button::State UITrainingPanel::button_pressed(uint16_t id)
    {
        if (id==0 || id==10) { toggle(); return Button::NORMAL; }
        if (id>=20 && id<=23 && !pending_) {
            int row=id-20; auto choices=available_skills; choices.insert(choices.begin(),-1);
            auto it=std::find(choices.begin(),choices.end(),rotation_skills[row]);
            size_t index=it==choices.end() ? 0 : (it-choices.begin()+1)%choices.size();
            rotation_skills[row]=choices[index];
            if (training_buff(rotation_skills[row])) {
                int skill=rotation_skills[row]; auto idstr=string_format::extend_id(skill,7);
                auto source=nl::nx::skill[string_format::extend_id(skill/10000,3)+".img"]["skill"][idstr]["level"][std::to_string(Stage::get().get_player().get_skilllevel(skill))];
                int duration=source["time"].get_integer(60);
                rotation_ms[row]=std::max(1000,std::min(600000,(duration>5 ? duration-5 : duration)*1000));
            } else rotation_ms[row]=1000;
        }
        else if (id>=30 && id<=33 && !pending_) {
            static const int periods[]={200,500,1000,2000,3000,5000,10000,20000,30000,60000,120000,180000,300000,600000};
            int& ms=rotation_ms[id-30]; auto it=std::upper_bound(std::begin(periods),std::end(periods),ms);
            ms=it==std::end(periods) ? periods[0] : *it;
        }
        else if (id==41 && !pending_ && status.fresh() && status.allowed) start_rotation();
        else if (id==8) request("sync");
        else if (id==9) { Stage::get().set_flight_mode(0); request("off"); }
        else if (id==6 || id==7 || id==11) Stage::get().set_flight_mode(Stage::get().get_flight_mode()==(id==6 ? 1 : 2) ? 0 : id==6 ? 1 : 2);
        else if (!pending_ && status.fresh() && status.allowed)
        {
            if (id==15 && status.rapid_known) {
                int rate=status.rapid_rate==0 ? 10 : status.rapid_rate==10 ? 20 : status.rapid_rate==20 ? 40 : 0;
                int skill=status.rapid_rate ? status.rapid_skill : available_skills[selected_skill];
                if (!rate) request("rapid off");
                else if (TrainingAutoAttack::rapid_supported(skill)) {
                    use_rotation=false;
                    request("rapid on "+std::to_string(rate)+" "+std::to_string(skill));
                }
            }
            if (id==12 && status.pet_vacuum_split_known) request(std::string("petvac mesos ")+(status.pet_vacuum_mesos ? "off":"on"));
            if (id==14 && status.pet_vacuum_split_known) request(std::string("petvac items ")+(status.pet_vacuum_items ? "off":"on"));
            if (id==1) request(std::string("invincible ")+(status.invincible ? "off":"on"));
            if (id==2) request(std::string("silence ")+(status.silence ? "off":"on"));
            if (id==3) request(std::string("vac ")+(status.vacuum ? "off":"on"));
            if (id==4) {
                if (status.auto_skill>=0) request("auto off");
                else if (use_rotation) start_rotation();
                else request("auto on "+std::to_string(available_skills[selected_skill]));
            }
            if (id==5 && status.auto_skill<0) { use_rotation=false; selected_skill=(selected_skill+1)%available_skills.size(); }
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
        for (auto id : {13,40})
            if (down && buttons.count(id) && buttons.at(id)->bounds(position).contains(at)) {
                rotation_view=id==13; refresh_skills(); layout(); update(); return Cursor::IDLE;
            }
        for (auto id : {6,7})
            if (down && buttons.count(id) && buttons.at(id)->bounds(position).contains(at))
            {
                const int selected = id==6 ? 1 : 2;
                const int mode = Stage::get().get_flight_mode()==selected ? 0 : selected;
                Stage::get().set_flight_mode(mode);
                if (mode && Stage::get().get_flight_mode()==mode) {
                    expanded_=false; layout(); update();
                }
                return Cursor::IDLE;
            }
        return UIElement::send_cursor(down,at);
    }
    void UITrainingPanel::remove_cursor() { held_=false; UIElement::remove_cursor(); }
}
