#include "crow_all.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <ctime>
#include <vector>
#include <algorithm>
#include <deque>
#include <iomanip>
#include <cmath>
#include <cstdlib>

using json = nlohmann::json;

// --- CONSTANTS ---
const long long DAY_SEC = 86400;
const long long HALF_DAY = 43200; 
const long long VIRTUE_REWARD = DAY_SEC; 
const long long ACTION_COOLDOWN = 72000;     // 20 Hours

// Dynamic DB Path
std::string get_db_path() {
    const char* env_p = std::getenv("DB_PATH");
    return env_p ? std::string(env_p) : "/data/db.json";
}
const std::string DB_FILE = get_db_path();

// --- DATA STRUCTURES ---

struct ActivityLog {
    std::string user_name;
    std::string action; 
    std::string message;
    time_t timestamp;
    std::string color; 
    long long change_delta;
    long long debt_snapshot;
};

struct User {
    std::string name;
    std::string id;
    std::string password;
    
    std::string vice;
    double target_interval_days;
    std::string virtue1_name;
    double promised_v1_weekly;
    std::string virtue2_name;
    double promised_v2_weekly;

    long long base_cost;     
    long long max_threshold; 
    
    long long debt_seconds;
    time_t last_update;      
    time_t last_v1;
    time_t last_v2;
    time_t lock_time;
    bool locked;
    int streak; 

    // Achievements
    time_t last_vice;
    int highest_clean_milestone; 
    int virtue_streak_days;
    time_t last_virtue_day_check;

    User() : debt_seconds(0), last_update(std::time(nullptr)), last_v1(0), last_v2(0), lock_time(0), locked(false), streak(0), last_vice(std::time(nullptr)), highest_clean_milestone(0), virtue_streak_days(0), last_virtue_day_check(0) {}

    User(std::string n, std::string p, std::string v, double days, std::string v1n, double v1f, std::string v2n, double v2f) 
        : name(n), password(p), vice(v), target_interval_days(days), 
          virtue1_name(v1n), promised_v1_weekly(v1f), virtue2_name(v2n), promised_v2_weekly(v2f),
          debt_seconds(0), last_update(std::time(nullptr)), last_v1(0), last_v2(0), lock_time(0), locked(false), streak(0),
          last_vice(std::time(nullptr)), highest_clean_milestone(0), virtue_streak_days(0), last_virtue_day_check(0)
    {
        id = n;
        std::transform(id.begin(), id.end(), id.begin(), ::tolower);
        calculate_math();
    }

    void calculate_math() {
        long long natural_decay = (long long)(target_interval_days * DAY_SEC);
        double weeks_in_interval = target_interval_days / 7.0;
        double total_virtues = weeks_in_interval * (promised_v1_weekly + promised_v2_weekly);
        double raw_work_capacity = total_virtues * (double)VIRTUE_REWARD;
        double raw_total = (double)natural_decay + raw_work_capacity;
        long long blocks = (long long)std::round(raw_total / (double)HALF_DAY);
        base_cost = blocks * HALF_DAY;
        if (base_cost < natural_decay) base_cost = natural_decay;
        max_threshold = (long long)(base_cost * 2.5);
    }
};

std::map<std::string, User> users;
std::deque<ActivityLog> activity_feed; 

std::string get_user_color(const std::string& name) {
    std::hash<std::string> hasher;
    size_t hash = hasher(name);
    int hue = hash % 360;
    return "hsl(" + std::to_string(hue) + ", 70%, 65%)";
}

// --- DATABASE FUNCTIONS ---

void save_db() {
    json j;
    j["users"] = json::object();
    for (auto const& [key, user] : users) {
        j["users"][key] = {
            {"name", user.name},
            {"password", user.password},
            {"vice", user.vice},
            {"target_interval_days", user.target_interval_days},
            {"virtue1_name", user.virtue1_name},
            {"promised_v1_weekly", user.promised_v1_weekly},
            {"virtue2_name", user.virtue2_name},
            {"promised_v2_weekly", user.promised_v2_weekly},
            {"base_cost", user.base_cost},
            {"max_threshold", user.max_threshold},
            {"debt_seconds", user.debt_seconds},
            {"last_update", user.last_update},
            {"last_v1", user.last_v1},
            {"last_v2", user.last_v2},
            {"lock_time", user.lock_time},
            {"locked", user.locked},
            {"streak", user.streak},
            {"last_vice", user.last_vice},
            {"clean_milestone", user.highest_clean_milestone},
            {"v_streak", user.virtue_streak_days},
            {"last_v_check", user.last_virtue_day_check}
        };
    }
    j["logs"] = json::array();
    for (const auto& log : activity_feed) {
        j["logs"].push_back({
            {"user", log.user_name},
            {"action", log.action},
            {"msg", log.message},
            {"ts", log.timestamp},
            {"col", log.color},
            {"delta", log.change_delta},
            {"snap", log.debt_snapshot}
        });
    }
    std::ofstream o(DB_FILE);
    o << j.dump(4);
}

void load_db() {
    std::ifstream i(DB_FILE);
    if (!i.is_open()) return;
    json j;
    i >> j;
    users.clear();
    for (auto& [key, val] : j["users"].items()) {
        User u;
        u.name = val["name"];
        u.id = key; 
        u.password = val["password"];
        u.vice = val.value("vice", "Vice");
        u.target_interval_days = val.value("target_interval_days", 7.0);
        u.virtue1_name = val.value("virtue1_name", "Virtue 1");
        u.promised_v1_weekly = val.value("promised_v1_weekly", 3.0);
        u.virtue2_name = val.value("virtue2_name", "Virtue 2");
        u.promised_v2_weekly = val.value("promised_v2_weekly", 5.0);
        u.base_cost = val.value("base_cost", 10 * DAY_SEC);
        u.max_threshold = val.value("max_threshold", 25 * DAY_SEC);
        u.debt_seconds = val["debt_seconds"];
        u.last_update = val["last_update"];
        u.last_v1 = val.value("last_v1", 0);
        u.last_v2 = val.value("last_v2", 0);
        u.lock_time = val.value("lock_time", 0);
        u.locked = val["locked"];
        u.streak = val.value("streak", 0);
        
        u.last_vice = val.value("last_vice", (long long)std::time(nullptr));
        u.highest_clean_milestone = val.value("clean_milestone", 0);
        u.virtue_streak_days = val.value("v_streak", 0);
        u.last_virtue_day_check = val.value("last_v_check", 0);

        users[key] = u;
    }
    activity_feed.clear();
    if (j.contains("logs")) {
        for (const auto& l : j["logs"]) {
            ActivityLog log;
            log.user_name = l["user"];
            log.action = l["action"];
            log.message = l["msg"];
            log.timestamp = l["ts"];
            log.color = l["col"];
            log.change_delta = l.value("delta", 0LL);
            log.debt_snapshot = l.value("snap", 0LL);
            activity_feed.push_back(log);
        }
    }
}

// --- LOGIC FUNCTIONS ---

void add_log(std::string user, std::string action, std::string msg, std::string color, long long delta, long long snapshot) {
    ActivityLog log;
    log.user_name = user;
    log.action = action;
    log.message = msg;
    log.timestamp = std::time(nullptr);
    log.color = color;
    log.change_delta = delta;
    log.debt_snapshot = snapshot;
    activity_feed.push_front(log); 
    if (activity_feed.size() > 100) activity_feed.pop_back(); 
    save_db();
}

void check_achievements(User& u) {
    time_t now = std::time(nullptr);
    double days_clean = std::difftime(now, u.last_vice) / 86400.0;
    int milestones[] = {5, 10, 25, 50, 100, 200, 300};
    
    for (int m : milestones) {
        if (days_clean >= m && u.highest_clean_milestone < m) {
            u.highest_clean_milestone = m;
            add_log(u.name, "achievement", "🏆 ACHIEVEMENT: Clean for " + std::to_string(m) + " days!", "#FFD700", 0, u.debt_seconds);
        }
    }
}

void update_decay(User& u) {
    if (u.locked) return;
    time_t now = std::time(nullptr);
    long long seconds_passed = (long long)std::difftime(now, u.last_update);
    if (u.debt_seconds > 0) {
        u.debt_seconds -= seconds_passed;
        if (u.debt_seconds < 0) u.debt_seconds = 0;
    }
    u.last_update = now;
    check_achievements(u);
}

void add_vice(User& u) {
    update_decay(u);
    if (u.locked) return;
    long long cost = u.base_cost;
    if (u.debt_seconds > 0) {
        cost = (long long)(u.base_cost * 1.5);
    }
    u.debt_seconds += cost;
    
    u.streak = 0; 
    u.last_vice = std::time(nullptr);
    u.highest_clean_milestone = 0;
    u.virtue_streak_days = 0;

    double days = (double)cost / (double)DAY_SEC;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << days;
    std::string msg = "Indulged in " + u.vice + " (+" + ss.str() + "d)";
    add_log(u.name, "vice", msg, "#ff5252", cost, u.debt_seconds);
    if (u.debt_seconds > u.max_threshold) {
        u.locked = true;
        u.lock_time = std::time(nullptr);
        add_log(u.name, "locked", "WENT BANKRUPT.", "#ff0000", 0, u.debt_seconds);
    }
    save_db();
}

bool perform_virtue(User& u, int virtue_num) {
    update_decay(u);
    if (u.locked) return false;
    time_t now = std::time(nullptr);
    time_t* last_track = (virtue_num == 1) ? &u.last_v1 : &u.last_v2;
    std::string v_name = (virtue_num == 1) ? u.virtue1_name : u.virtue2_name;
    if (std::difftime(now, *last_track) < ACTION_COOLDOWN) return false;
    
    struct tm* t_now = std::localtime(&now);
    struct tm* t_last = std::localtime(&u.last_virtue_day_check);
    bool new_day = (t_now->tm_yday != t_last->tm_yday || t_now->tm_year != t_last->tm_year);
    if (new_day) {
        u.virtue_streak_days++;
        u.last_virtue_day_check = now;
        int milestones[] = {10, 25, 50, 100};
        for (int m : milestones) {
            if (u.virtue_streak_days == m) {
                add_log(u.name, "achievement", "🔥 STREAK: " + std::to_string(m) + " days of virtues!", "#FFD700", 0, u.debt_seconds);
            }
        }
    }

    long long removed = 0;
    if (u.debt_seconds > 0) {
        removed = VIRTUE_REWARD;
        if (u.debt_seconds < removed) removed = u.debt_seconds;
        u.debt_seconds -= removed;
    }
    *last_track = now;
    std::string col = (virtue_num == 1) ? "#2196F3" : "#9c27b0";
    std::string action_key = (virtue_num == 1) ? "virtue1" : "virtue2";
    add_log(u.name, action_key, "Completed: " + v_name + " (-1d)", col, -removed, u.debt_seconds);
    save_db();
    return true;
}

void reset_user(User& u, std::string verifier) {
    time_t now = std::time(nullptr);
    long long time_served = (long long)std::difftime(now, u.lock_time);
    u.debt_seconds = u.base_cost - time_served;
    if (u.debt_seconds < 0) u.debt_seconds = 0;
    u.locked = false;
    u.last_update = now;
    u.streak++; 
    add_log(u.name, "reset", "Bailed out by " + verifier + ".", "#4CAF50", 0, u.debt_seconds);
    save_db();
}

bool perform_undo(User& u) {
    if (activity_feed.empty()) return false;
    
    // Check for "LOCKED" message first (if I am currently locked)
    if (u.locked && activity_feed.front().action == "locked" && activity_feed.front().user_name == u.name) {
        u.locked = false;
        u.lock_time = 0;
        activity_feed.pop_front(); // Remove the "WENT BANKRUPT" message
        // Do NOT return true yet. We must continue to undo the VICE action that caused it.
        // If the feed is now empty (shouldn't be), return.
        if (activity_feed.empty()) { save_db(); return true; }
    }

    ActivityLog last = activity_feed.front();
    
    if (last.user_name == u.name) {
        time_t now = std::time(nullptr);
        // 10 minute undo window
        if (std::difftime(now, last.timestamp) < 600) { 
            
            // Revert Debt
            u.debt_seconds -= last.change_delta;
            if (u.debt_seconds < 0) u.debt_seconds = 0;
            
            // Revert Cooldowns (Reset to 0)
            if (last.action == "virtue1") u.last_v1 = 0;
            if (last.action == "virtue2") u.last_v2 = 0;
            
            activity_feed.pop_front();
            save_db();
            return true;
        }
    }
    return false;
}

std::string get_logged_in_user(const crow::request& req) {
    std::string cookie_val = req.get_header_value("Cookie");
    if (cookie_val.find("user=") != std::string::npos) {
        std::string name = cookie_val.substr(cookie_val.find("user=") + 5);
        size_t end = name.find(';');
        if (end != std::string::npos) name = name.substr(0, end);
        std::string decoded_name = name;
        std::replace(decoded_name.begin(), decoded_name.end(), '+', ' ');
        std::string lower_id = decoded_name;
        std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
        if (users.find(lower_id) != users.end()) return lower_id;
    }
    return "";
}

std::string get_form_value(const std::string& body, const std::string& key) {
    std::string search = key + "=";
    size_t start = body.find(search);
    if (start == std::string::npos) return "";
    start += search.length();
    size_t end = body.find('&', start);
    if (end == std::string::npos) end = body.length();
    std::string val = body.substr(start, end - start);
    std::replace(val.begin(), val.end(), '+', ' ');
    return val;
}

bool is_same_day(time_t t1, time_t t2) {
    struct tm* tm1 = std::localtime(&t1);
    struct tm tm1_copy = *tm1; 
    struct tm* tm2 = std::localtime(&t2);
    return (tm1_copy.tm_year == tm2->tm_year && tm1_copy.tm_yday == tm2->tm_yday);
}

std::string get_day_name(time_t t) {
    char buffer[10];
    struct tm* tm_info = std::localtime(&t);
    strftime(buffer, 10, "%a", tm_info); 
    return std::string(buffer);
}

// --- HTML RENDERERS ---

std::string render_calendar(const std::string& username) {
    time_t now = std::time(nullptr);
    std::string html = "<div style='display:flex; justify-content:space-between; margin-top:15px; background:rgba(0,0,0,0.2); padding:10px; border-radius:8px;'>";
    for (int i = 6; i >= 0; i--) {
        time_t day_time = now - (i * 86400);
        std::string day_label = (i == 0) ? "Today" : get_day_name(day_time);
        bool has_vice = false;
        int virtue_count = 0;
        for (const auto& log : activity_feed) {
            if (log.user_name == username) {
                if (is_same_day(log.timestamp, day_time)) {
                    if (log.action == "vice") has_vice = true;
                    if (log.action == "virtue1" || log.action == "virtue2") virtue_count++;
                }
            }
        }
        html += "<div style='text-align:center; flex:1; display:flex; flex-direction:column; align-items:center;'>";
        html += "<div style='font-size:0.65em; color:#666; margin-bottom:5px; text-transform:uppercase;'>" + day_label + "</div>";
        html += "<div style='background:rgba(255,255,255,0.03); width:30px; height:40px; border-radius:4px; display:flex; flex-direction:column; justify-content:flex-end; align-items:center; padding:3px; gap:2px; border:1px solid rgba(255,255,255,0.05);'>";
            
            if (virtue_count > 0) {
                for(int k=0; k<std::min(virtue_count, 4); k++) {
                    html += "<div style='width:6px; height:6px; background:#4CAF50; border-radius:50%; box-shadow:0 0 5px rgba(76,175,80,0.3);'></div>";
                }
            } else if (!has_vice) {
                html += "<div style='width:4px; height:4px; background:#333; border-radius:50%; margin-bottom:auto; margin-top:auto;'></div>";
            }

            if (has_vice) {
                html += "<div style='width:6px; height:6px; background:#ff5252; border-radius:50%; margin-top:2px; opacity:0.9;'></div>";
            }

        html += "</div></div>";
    }
    html += "</div>";
    
    // GRAPH
    html += R"(
    <div style="margin-top:20px; background:rgba(0,0,0,0.2); padding:10px; border-radius:8px;">
        <canvas id="debtChart" height="150" style="width:100%;"></canvas>
    </div>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script>
        const canvas = document.getElementById('debtChart');
        if (canvas) {
            const ctx = canvas.getContext('2d');
            const data = {
                labels: [],
                datasets: [{
                    label: 'Debt',
                    data: [],
                    borderColor: '#4CAF50',
                    backgroundColor: (context) => {
                        const bg = context.chart.ctx.createLinearGradient(0, 0, 0, 150);
                        bg.addColorStop(0, 'rgba(76, 175, 80, 0.4)');
                        bg.addColorStop(1, 'rgba(76, 175, 80, 0.0)');
                        return bg;
                    },
                    borderWidth: 2,
                    tension: 0.3,
                    fill: true,
                    pointRadius: 0
                }]
            };
            const logs = [
    )";
    
    std::vector<ActivityLog> user_logs;
    for(const auto& log : activity_feed) {
        if(log.user_name == username && (log.action == "vice" || log.action == "virtue1" || log.action == "virtue2" || log.action == "reset")) {
            user_logs.push_back(log);
        }
    }
    std::reverse(user_logs.begin(), user_logs.end()); 
    
    for(const auto& log : user_logs) {
        double debt_days = (double)log.debt_snapshot / (double)DAY_SEC;
        html += "{ y: " + std::to_string(debt_days) + " },";
    }
    
    html += R"(
            ];
            logs.forEach((log, index) => {
                data.labels.push(index + 1);
                data.datasets[0].data.push(log.y);
            });
            new Chart(ctx, {
                type: 'line',
                data: data,
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { display: false },
                        y: { beginAtZero: true, grid: { color: '#333' }, ticks: { color: '#888' } }
                    },
                    plugins: { legend: { display: false } },
                    animation: false
                }
            });
        }
    </script>
    )";
    return html;
}

std::string render_signup_wizard(std::string error = "") {
    std::string html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>The Pledge</title>
        <style>
            body { background: #121212; background-image: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI0IiBoZWlnaHQ9IjQiPgo8cmVjdCB3aWR0aD0iNCIgaGVpZ2h0PSI0IiBmaWxsPSIjMTIxMjEyIi8+CjxyZWN0IHdpZHRoPSIxIiBoZWlnaHQ9IjEiIGZpbGw9IiMxYTFhMWEiLz4KPC9zdmc+'); color: #e0e0e0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
            .box { background: rgba(30, 30, 30, 0.8); backdrop-filter: blur(20px); -webkit-backdrop-filter: blur(20px); border: 1px solid rgba(255, 255, 255, 0.08); padding: 40px; border-radius: 16px; width: 420px; box-shadow: 0 20px 50px rgba(0,0,0,0.5); }
            h2 { color: #fff; text-align: center; margin-top: 0; letter-spacing: 2px; text-transform:uppercase; font-size:1.4em; margin-bottom: 20px;}
            
            .step { display: none; }
            .step.active { display: block; animation: fadein 0.4s; }
            
            label { font-size: 0.75em; color: #888; text-transform: uppercase; letter-spacing: 1px; display: block; margin-bottom: 8px; font-weight:700;}
            
            .input-wrapper { margin-bottom: 20px; }
            input, select { width: 100%; padding: 16px; background: rgba(0,0,0,0.3); border: 1px solid #444; color: white; border-radius: 8px; box-sizing: border-box; font-size: 1.1em; transition:0.2s; -webkit-appearance: none; }
            input:focus, select:focus { border-color: #4CAF50; outline: none; background: rgba(0,0,0,0.5); }
            
            input::-webkit-outer-spin-button, input::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }
            input[type=number] { -moz-appearance: textfield; }

            .combo-input { display: flex; gap: 10px; align-items: baseline; }
            .combo-input input { flex: 1; text-align:center; }
            .slash { font-size: 1.5em; color: #444; font-weight: 300; }
            .combo-input select { flex: 2; text-align:center; text-align-last:center; }

            .dynamic-text { font-size:0.85em; color:#888; margin-top:10px; text-align:center; font-style: italic; min-height: 1.2em; transition: color 0.3s; }

            p.desc { font-size: 0.95em; line-height: 1.6; color: #bbb; margin-bottom: 30px; text-align:left; }
            
            .btn-row { display: flex; justify-content: space-between; margin-top: 30px; }
            button { padding: 14px 24px; background: #4CAF50; border: none; color: white; font-weight: bold; cursor: pointer; border-radius: 8px; font-size: 1em; text-transform:uppercase; letter-spacing:1px; transition:0.1s; }
            button:active { transform: scale(0.98); }
            button.secondary { background: transparent; border: 1px solid #444; color: #888; }
            button.secondary:hover { border-color: #666; color: #ccc; }
            
            .review-item { background: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; margin-bottom: 10px; border-left: 3px solid #4CAF50; }
            .review-label { font-size: 0.7em; color: #666; text-transform: uppercase; }
            .review-val { font-size: 1.1em; color: #fff; font-weight: bold; }
            .review-sub { font-size:0.9em; color:#888; }
            
            @keyframes fadein { from { opacity:0; transform:translateY(5px); } to { opacity:1; transform:translateY(0); } }
            .error { background: #3d1a1a; color: #ff9898; padding: 12px; border-radius: 8px; margin-bottom: 20px; text-align: center; border: 1px solid #5e2a2a; }
            .info-icon { display:inline-block; width:14px; height:14px; border:1px solid #555; color:#555; border-radius:50%; text-align:center; line-height:13px; font-size:0.75em; margin-left:6px; cursor:help; }
        </style>
        <script>
            let currentStep = 1;
            function showStep(n) {
                document.querySelectorAll('.step').forEach(s => s.classList.remove('active'));
                document.getElementById('step' + n).classList.add('active');
                currentStep = n;
                if (n === 4) populateReview();
                
                if (n === 2) updateText('I will indulge', 'vice');
                if (n === 3) { updateText('I will complete this goal', 'v1'); updateText('I will complete this goal', 'v2'); }
            }
            function next() { showStep(currentStep + 1); }
            function back() { showStep(currentStep - 1); }

            function numToWord(n) {
                const words = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"];
                if (n >= 0 && n <= 10) return words[n];
                return n;
            }

            function updateText(prefix, id) {
                let freq = parseInt(document.getElementById(id + '-freq').value);
                let sel = document.getElementById(id + '-per');
                let per = sel.options[sel.selectedIndex].text;
                
                let countWord = numToWord(freq);
                let timeStr = (freq === 1) ? "time" : "times";
                let perStr = per.toLowerCase();
                
                document.getElementById(id + '-text').innerHTML = prefix + " <b style='color:#ccc'>" + countWord + " " + timeStr + "</b> per " + perStr;
            }

            function populateReview() {
                document.getElementById('r-name').innerText = document.querySelector('input[name="name"]').value;
                let vice = document.querySelector('input[name="vice"]').value;
                document.getElementById('r-vice').innerText = vice;
                
                let viceFreq = document.getElementById('vice-freq').value;
                let vicePer = document.getElementById('vice-per').options[document.getElementById('vice-per').selectedIndex].text.toLowerCase();
                document.getElementById('r-days').innerText = viceFreq + "x / " + vicePer;

                let v1 = document.querySelector('input[name="v1name"]').value;
                let v1f = document.getElementById('v1-freq').value;
                let v1p = document.getElementById('v1-per').options[document.getElementById('v1-per').selectedIndex].text.toLowerCase();
                
                let v2 = document.querySelector('input[name="v2name"]').value;
                let v2f = document.getElementById('v2-freq').value;
                let v2p = document.getElementById('v2-per').options[document.getElementById('v2-per').selectedIndex].text.toLowerCase();

                document.getElementById('r-v1').innerText = v1;
                document.getElementById('r-v1-sub').innerText = v1f + "x / " + v1p;
                document.getElementById('r-v2').innerText = v2;
                document.getElementById('r-v2-sub').innerText = v2f + "x / " + v2p;
            }
        </script>
    </head>
    <body>
        <div class="box">
            <form action="/signup" method="POST">
                )=====";
                if (!error.empty()) html += "<div class='error'>" + error + "</div>";
                html += R"=====(
                
                <div id="step1" class="step active">
                    <h2>EARN YOUR VICES</h2>
                    <p class="desc">
                        This app will help you <b>moderate</b> your vices through virtues.<br><br>
                        1. Define the negative habit you wish to control.<br>
                        2. Define the positive habits that will pay for it.<br>
                        3. Sign the contract
                    </p>
                    <div class="input-wrapper">
                        <label>Who are you?</label>
                        <input type="text" name="name" placeholder="Name" required autocomplete="off">
                    </div>
                    <div class="input-wrapper">
                        <label>Password</label>
                        <input type="password" name="password" placeholder="Password" required>
                    </div>
                    <div class="btn-row" style="justify-content:center">
                        <button type="button" onclick="next()">Start Contract</button>
                    </div>
                    <div style="text-align:center; margin-top:20px"><a href="/login" style="color:#666; text-decoration:none; font-size:0.8em; text-transform:uppercase;">Back to Login</a></div>
                </div>

                <div id="step2" class="step">
                    <h2>The Vice</h2>
                    <p class="desc">What is your biggest vice? How often will you indulge?<br>
                                    This will help define how much you owe for every indulgence. </p>
                    
                    <div class="input-wrapper">
                        <label title="The specific activity (e.g. Smoking Weed, Ordering UberEats)">Vice Name <span class="info-icon">?</span></label>
                        <input type="text" name="vice" placeholder="e.g. Weed" required>
                    </div>
                    
                    <div class="input-wrapper">
                        <label title="Your maximum allowed indulgence rate.">Schedule Limit <span class="info-icon">?</span></label>
                        <div class="combo-input">
                            <input type="number" id="vice-freq" name="vice_freq" value="1" min="1" oninput="updateText('I will indulge', 'vice')">
                            <div class="slash">/</div>
                            <select id="vice-per" name="vice_per" onchange="updateText('I will indulge', 'vice')">
                                <option value="1">Day</option>
                                <option value="7" selected>Week</option>
                                <option value="30">Month</option>
                                <option value="365">Year</option>
                            </select>
                        </div>
                        <div id="vice-text" class="dynamic-text">I will indulge one time per week</div>
                    </div>
                    
                    <div class="btn-row">
                        <button type="button" class="secondary" onclick="back()">Back</button>
                        <button type="button" onclick="next()">Next</button>
                    </div>
                </div>

                <div id="step3" class="step">
                    <h2>The Virtues</h2>
                    <p class="desc">Pick two virtues to earn your indulgence. These are the habits that you want to have.</p>
                    
                    <div class="input-wrapper">
                        <label title="A daily/weekly positive habit (e.g. Gym)">Virtue #1 <span class="info-icon">?</span></label>
                        <input type="text" name="v1name" placeholder="e.g. Gym" required>
                        <div class="combo-input" style="margin-top:5px">
                            <input type="number" id="v1-freq" name="v1_freq" value="3" min="1" oninput="updateText('I will complete this goal', 'v1')">
                            <div class="slash">/</div>
                            <select id="v1-per" name="v1_per" onchange="updateText('I will complete this goal', 'v1')">
                                <option value="1">Day</option>
                                <option value="7" selected>Week</option>
                            </select>
                        </div>
                        <div id="v1-text" class="dynamic-text">I will complete this goal three times per week</div>
                    </div>

                    <div class="input-wrapper">
                        <label title="Another positive habit (e.g. Studying, Cleaning)">Virtue #2 <span class="info-icon">?</span></label>
                        <input type="text" name="v2name" placeholder="e.g. Study" required>
                        <div class="combo-input" style="margin-top:5px">
                            <input type="number" id="v2-freq" name="v2_freq" value="5" min="1" oninput="updateText('I will complete this goal', 'v2')">
                            <div class="slash">/</div>
                            <select id="v2-per" name="v2_per" onchange="updateText('I will complete this goal', 'v2')">
                                <option value="1">Day</option>
                                <option value="7" selected>Week</option>
                            </select>
                        </div>
                        <div id="v2-text" class="dynamic-text">I will complete this goal five times per week</div>
                    </div>

                    <div class="btn-row">
                        <button type="button" class="secondary" onclick="back()">Back</button>
                        <button type="button" onclick="next()">Next</button>
                    </div>
                </div>

                <div id="step4" class="step">
                    <h2>The Pledge</h2>
                    <p class="desc" style="margin-bottom:15px">I, <span id="r-name" style="color:#fff; font-weight:bold"></span>, hereby agree to the following terms of moderation:</p>
                    
                    <div class="review-item">
                        <div class="review-label">Moderating</div>
                        <div class="review-val" id="r-vice">...</div>
                        <div class="review-sub" id="r-days">...</div>
                    </div>
                    
                    <div class="review-item" style="border-left-color: #2196F3">
                        <div class="review-label">Powered By</div>
                        <div class="review-val" id="r-v1">...</div>
                        <div class="review-sub" id="r-v1-sub" style="margin-bottom:8px">...</div>
                        
                        <div class="review-val" id="r-v2">...</div>
                        <div class="review-sub" id="r-v2-sub">...</div>
                    </div>

                    <div class="btn-row">
                        <button type="button" class="secondary" onclick="back()">Back</button>
                        <button type="submit">Sign Contract</button>
                    </div>
                </div>

            </form>
        </div>
    </body>
    </html>
    )=====";
    return html;
}

std::string render_edit_page(const User& u) {
    std::string html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Edit Contract</title>
        <style>
            body { background: #121212; background-image: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI0IiBoZWlnaHQ9IjQiPgo8cmVjdCB3aWR0aD0iNCIgaGVpZ2h0PSI0IiBmaWxsPSIjMTIxMjEyIi8+CjxyZWN0IHdpZHRoPSIxIiBoZWlnaHQ9IjEiIGZpbGw9IiMxYTFhMWEiLz4KPC9zdmc+'); color: #e0e0e0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
            .box { background: rgba(30, 30, 30, 0.8); backdrop-filter: blur(20px); -webkit-backdrop-filter: blur(20px); border: 1px solid rgba(255, 255, 255, 0.08); padding: 40px; border-radius: 16px; width: 400px; }
            h2 { color: #4CAF50; text-align: center; margin-top: 0; text-transform:uppercase; letter-spacing:1px; }
            
            label { font-size: 0.75em; color: #888; text-transform: uppercase; letter-spacing: 1px; display: block; margin-bottom: 5px; font-weight:700;}
            
            .input-wrapper { margin-bottom: 20px; }
            input, select { width: 100%; padding: 14px; background: rgba(0,0,0,0.3); border: 1px solid #444; color: white; border-radius: 8px; box-sizing: border-box; font-size: 1.1em; -webkit-appearance: none; }
            
            .combo-input { display: flex; gap: 10px; align-items: baseline; }
            .combo-input input { flex: 1; text-align:center; }
            .slash { font-size: 1.5em; color: #444; font-weight: 300; }
            .combo-input select { flex: 2; text-align:center; text-align-last:center; }

            input::-webkit-outer-spin-button, input::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }
            input[type=number] { -moz-appearance: textfield; }
            
            button { width: 100%; padding: 14px; background: #4CAF50; border: none; color: white; font-weight: bold; cursor: pointer; border-radius: 8px; margin-top: 10px; font-size: 1em; text-transform:uppercase; letter-spacing:1px; transition:0.1s; }
            button:active { transform: scale(0.98); }
            
            .del-btn { background: transparent; border: 1px solid #ff5252; color: #ff5252; margin-top: 20px; font-size: 0.9em; }
            .del-btn:hover { background: rgba(255, 82, 82, 0.1); }
            
            a { display:block; text-align:center; margin-top:15px; color:#666; text-decoration:none; text-transform:uppercase; font-size:0.8em; }
        </style>
    </head>
    <body>
        <div class="box">
            <h2>Amend Contract</h2>
            
            <!-- MAIN EDIT FORM -->
            <form action="/edit" method="POST">
                
                <div class="input-wrapper">
                    <label>Vice Name</label>
                    <input type="text" name="vice" value=")=====" + u.vice + R"=====(">
                </div>
                
                <div class="input-wrapper">
                    <label>Schedule Limit</label>
                    <div class="combo-input">
                        <input type="number" name="vice_freq" value="1" min="1">
                        <div class="slash">/</div>
                        <select name="vice_per">
                            <option value="1">Day</option>
                            <option value="7" selected>Week</option>
                            <option value="30">Month</option>
                        </select>
                    </div>
                    <div style="font-size:0.7em; color:#666; margin-top:5px; text-align:center">Resetting to defaults. Please re-enter desired rate.</div>
                </div>
                
                <hr style="border:0; border-top:1px solid #444; margin:25px 0;">

                <div class="input-wrapper">
                    <label>Virtue #1</label>
                    <input type="text" name="v1name" value=")=====" + u.virtue1_name + R"=====(">
                    <div class="combo-input" style="margin-top:5px">
                        <input type="number" name="v1_freq" value="3" min="1">
                        <div class="slash">/</div>
                        <select name="v1_per">
                            <option value="1">Day</option>
                            <option value="7" selected>Week</option>
                        </select>
                    </div>
                </div>

                <div class="input-wrapper">
                    <label>Virtue #2</label>
                    <input type="text" name="v2name" value=")=====" + u.virtue2_name + R"=====(">
                    <div class="combo-input" style="margin-top:5px">
                        <input type="number" name="v2_freq" value="5" min="1">
                        <div class="slash">/</div>
                        <select name="v2_per">
                            <option value="1">Day</option>
                            <option value="7" selected>Week</option>
                        </select>
                    </div>
                </div>
                
                <hr style="border:0; border-top:1px solid #444; margin:25px 0;">
                
                <div class="input-wrapper">
                    <label>New Password</label>
                    <input type="password" name="new_password" placeholder="Leave blank to keep current">
                </div>

                <button type="submit">Save Changes</button>
            </form>
            
            <!-- SEPARATE DELETE FORM -->
            <form action="/delete_account" method="POST" onsubmit="return confirm('Are you sure? This cannot be undone.');">
                <button type="submit" class="del-btn">Delete Account</button>
            </form>
            
            <a href="/">Cancel</a>
        </div>
    </body>
    </html>
    )=====";
    return html;
}

std::string render_login(std::string error = "") {
    std::string html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>reCurrency Login</title>
        <style>
            body { background: #121212; background-image: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI0IiBoZWlnaHQ9IjQiPgo8cmVjdCB3aWR0aD0iNCIgaGVpZ2h0PSI0IiBmaWxsPSIjMTIxMjEyIi8+CjxyZWN0IHdpZHRoPSIxIiBoZWlnaHQ9IjEiIGZpbGw9IiMxYTFhMWEiLz4KPC9zdmc+'); color: #e0e0e0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
            .box { background: rgba(30, 30, 30, 0.8); backdrop-filter: blur(20px); -webkit-backdrop-filter: blur(20px); border: 1px solid rgba(255, 255, 255, 0.08); padding: 40px; border-radius: 16px; width: 320px; text-align: center; box-shadow: 0 20px 50px rgba(0,0,0,0.5); }
            h2 { color: #4CAF50; margin-top: 0; letter-spacing:-1px; }
            input { width: 100%; padding: 14px; margin: 8px 0; background: rgba(0,0,0,0.3); border: 1px solid #444; color: white; border-radius: 8px; box-sizing: border-box; transition: 0.2s; font-size: 1em; }
            input:focus { border-color: #4CAF50; outline: none; background: rgba(0,0,0,0.5); }
            button { width: 100%; padding: 14px; background: #4CAF50; border: none; color: white; font-weight: bold; cursor: pointer; border-radius: 8px; margin-top: 20px; font-size: 1em; transition:0.1s; }
            button:active { transform: scale(0.98); }
            .error { color: #ff5252; font-size: 0.9em; margin-bottom: 10px; }
            a { color: #888; text-decoration: none; font-size: 0.9em; }
        </style>
    </head>
    <body>
        <div class="box">
            <h2>reCurrency</h2>
            )";
            if (!error.empty()) html += "<div class='error'>" + error + "</div>";
            html += R"(
            <form action="/login" method="POST">
                <input type="text" name="name" placeholder="Username" required autocomplete="off">
                <input type="password" name="password" placeholder="Password" required>
                <button type="submit">Access Account</button>
            </form>
            <div style="margin-top:25px">
                <a href="/signup">New here? <b style="color:#ccc">Sign Contract</b></a>
            </div>
        </div>
    </body>
    </html>
    )";
    return html;
}

std::string render_feed() {
    std::string html = "<div class='feed-container'><h3>TRANSACTIONS</h3><div class='feed'>";
    for (const auto& log : activity_feed) {
        time_t now = std::time(nullptr);
        int diff = (int)difftime(now, log.timestamp);
        std::string time_str;
        if (diff < 60) time_str = "Now";
        else if (diff < 3600) time_str = std::to_string(diff/60) + "m";
        else if (diff < 86400) time_str = std::to_string(diff/3600) + "h";
        else time_str = std::to_string(diff/86400) + "d";

        html += "<div class='log-item' style='border-left: 2px solid " + log.color + "'>";
        html += "<div class='log-head'><span class='log-user'>" + log.user_name + "</span> <span class='log-time'>" + time_str + "</span></div>";
        html += "<div class='log-msg'>" + log.message + "</div>";
        html += "</div>";
    }
    html += "</div></div>";
    return html;
}

std::string render_dashboard(std::string current_user_id) {
    for (auto& [key, user] : users) update_decay(user);

    std::string html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>reCurrency</title>
        <style>
            body { background: #121212; background-image: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI0IiBoZWlnaHQ9IjQiPgo8cmVjdCB3aWR0aD0iNCIgaGVpZ2h0PSI0IiBmaWxsPSIjMTIxMjEyIi8+CjxyZWN0IHdpZHRoPSIxIiBoZWlnaHQ9IjEiIGZpbGw9IiMxYTFhMWEiLz4KPC9zdmc+'); color: #e0e0e0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; }
            
            .header { text-align: center; margin-bottom: 25px; }
            .header h2 { color: #4CAF50; letter-spacing: -1px; margin: 0; font-weight:800; font-size:1.8em; }
            
            .hero-card { padding: 30px; margin-bottom: 25px; border-radius: 20px; background: rgba(30, 30, 30, 0.6); backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px); border: 1px solid rgba(255, 255, 255, 0.08); box-shadow: 0 4px 30px rgba(0, 0, 0, 0.1); position:relative; overflow: hidden; }
            .hero-card::before { content: ""; position: absolute; top: -50%; left: -50%; width: 200%; height: 200%; background: radial-gradient(circle, rgba(76, 175, 80, 0.05) 0%, rgba(0,0,0,0) 70%); pointer-events: none; }
            .hero-card.locked { background: #251010; border-color: #ff4444; }
            .hero-card.locked::before { background: radial-gradient(circle, rgba(255, 82, 82, 0.05) 0%, rgba(0,0,0,0) 70%); }
            
            .tag { 
                font-size: 0.9em; 
                text-transform: uppercase; 
                color: #888; 
                letter-spacing: 0.5px; 
                font-weight: 700; 
                display: flex; 
                justify-content: space-between; 
                align-items: center; 
                margin-bottom: 15px;
            }

            .header-right {
                display: flex;
                align-items: center; 
                gap: 12px;
            }

            .moderating-badge { 
                background: #2c2c2c; 
                padding: 6px 10px; 
                border-radius: 6px; 
                color: #bbb; 
                font-size: 0.85em; 
                text-transform: none; 
                border: 1px solid #333; 
                white-space: nowrap; 
            }
            
            .edit-btn { 
                text-decoration: none; 
                color: #444; 
                font-size: 1.4em; 
                line-height: 1; 
                transition: color 0.2s; 
                display: flex; 
                align-items: center;
            }
            .edit-btn:hover { color: #fff; }

            .household-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 45px; }
            .mini-card { background: rgba(30, 30, 30, 0.6); backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px); border: 1px solid rgba(255, 255, 255, 0.08); padding: 15px; border-radius: 12px; position:relative; overflow:hidden; transition: transform 0.2s; }
            .mini-card:hover { transform: translateY(-2px); }
            .mini-card::before { content:''; position:absolute; top:0; left:0; width:100%; height:4px; background: var(--accent); }
            .mini-card.locked { border-color: #ff4444; background: #251010; }
            
            .timer { font-size: 2.8em; font-weight: 800; margin: 15px 0; color: #fff; text-align: center; font-variant-numeric: tabular-nums; letter-spacing: -1px; -webkit-font-smoothing: antialiased; }
            .mini-timer { font-size: 1.2em; font-weight: 700; color: #ccc; margin: 5px 0; font-variant-numeric: tabular-nums; }
            
            .streak { color: #FFD700; font-size: 1.1em; }
            
            .btn { width: 100%; padding: 14px; border: none; border-radius: 10px; font-weight: 700; cursor: pointer; color: white; margin-top: 5px; font-size: 0.95em; transition: 0.1s; }
            .btn:active { transform: scale(0.98); }
            .btn-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 5px; }
            
            .smoke-btn { background: #ff5252; }
            .virtue1-btn { background: #2196F3; }
            .virtue2-btn { background: #9c27b0; }
            .punishment-btn { background: #ff5252; color: white; font-size: 0.8em; padding: 8px; }
            
            .progress-bg { height: 8px; background: #333; border-radius: 4px; margin: 20px 0; overflow: hidden; }
            .progress-fill { height: 100%; transition: width 0.5s; }
            
            /* SMOOTH DETAILS ANIMATION (JS Controlled) */
            details { margin-top: 25px; border-top: 1px solid #333; padding-top: 15px; overflow: hidden; transition: max-height 0.3s ease-out; max-height: 30px; }
            details[open] { max-height: 500px; transition: max-height 0.5s ease-in; }
            summary { cursor: pointer; color: #666; font-size: 0.8em; text-transform: uppercase; letter-spacing: 1px; outline: none; font-weight:700; list-style: none; }
            summary::-webkit-details-marker { display: none; }
            summary:hover { color: #888; }
            
            /* Load animation suppressor */
            .preload * { transition: none !important; }

            .feed-container { margin: 20px 0; }
            .feed-container h3 { color: #666; font-size: 0.8em; letter-spacing: 1px; margin-bottom: 5px; }
            .feed { background: #161616; border-radius: 12px; height: 160px; overflow-y: auto; padding: 12px; border: 1px solid #333; }
            .log-item { padding: 8px 10px; margin-bottom: 8px; background: #222; border-radius: 6px; }
            .log-head { display: flex; justify-content: space-between; font-size: 0.75em; margin-bottom: 2px; }
            .log-user { font-weight: bold; color: #ccc; }
            .log-time { color: #666; }
            .log-msg { color: #aaa; font-size: 0.9em; }
            
            .logout { text-align: center; margin-top: 40px; }
            .logout a { color: #666; text-decoration: none; font-size: 0.8em; }
        </style>
        <script>
            function formatTime(seconds) {
                if (seconds <= 0) return "<span style='color:#4CAF50'>CLEAN</span>";
                let d = Math.floor(seconds / 86400);
                let h = Math.floor((seconds % 86400) / 3600);
                let m = Math.floor((seconds % 3600) / 60);
                let s = Math.floor(seconds % 60);
                return d + "d " + (h<10?"0"+h:h) + ":" + (m<10?"0"+m:m) + ":" + (s<10?"0"+s:s);
            }
            function startTimers() {
                // Remove preload class to enable transitions
                document.body.classList.remove('preload');

                const timers = document.querySelectorAll('.timer, .mini-timer');
                timers.forEach(t => {
                    let seconds = parseInt(t.getAttribute('data-seconds'));
                    if (isNaN(seconds)) return; 
                    t.innerHTML = formatTime(seconds);
                    setInterval(() => {
                        if (seconds > 0) seconds--;
                        t.innerHTML = formatTime(seconds);
                    }, 1000);
                });
                
                // NO-ANIMATION RESTORE LOGIC
                const details = document.querySelector("details");
                if(details) {
                    const isOpen = localStorage.getItem("insightsOpen");
                    
                    // If stored open, force it open WITHOUT animation logic kicking in (via preload class)
                    if (isOpen === "true") {
                        details.setAttribute("open", "");
                        details.style.maxHeight = "500px"; 
                    }
                    
                    details.querySelector("summary").addEventListener("click", (event) => {
                        event.preventDefault(); 
                        if(details.hasAttribute("open")) {
                            // Closing animation
                            details.style.maxHeight = "30px";
                            localStorage.setItem("insightsOpen", "false");
                            setTimeout(() => details.removeAttribute("open"), 300);
                        } else {
                            // Opening animation
                            details.setAttribute("open", "");
                            // Force reflow
                            void details.offsetWidth; 
                            details.style.maxHeight = "500px";
                            localStorage.setItem("insightsOpen", "true");
                        }
                    });
                }
            }
            window.onload = startTimers;
        </script>
    </head>
    <body class="preload">
        <div class="header"><h2>reCurrency</h2></div>
    )";

    // 1. ME
    if (users.count(current_user_id)) {
        User& u = users[current_user_id];
        
        html += "<div class='hero-card " + std::string(u.locked?"locked":"") + "'>";
        
        html += "<div class='tag'>";
        html += "<span>" + u.name + "</span>"; 
        
        html += "<div class='header-right'>";
        html += "<span class='moderating-badge'>Moderating: " + u.vice + "</span>";
        html += "<a href='/edit' class='edit-btn'>⚙</a>";
        html += "</div></div>";
        
        if (u.locked) {
            html += "<div class='timer' style='color:#ff5252'>BANKRUPT</div>";
            html += "<div style='color:#ff9898; text-align:center; font-size:0.9em;'>Account Frozen. Awaiting Bail Out.</div>";
        } else {
            html += "<div class='timer' data-seconds='" + std::to_string(u.debt_seconds) + "'>...</div>";
            
            double pct = (double)u.debt_seconds / (double)u.max_threshold * 100.0;
            if (pct>100) pct=100;
            std::string col = (u.debt_seconds > u.base_cost) ? "#ff9800" : "#4CAF50";
            
            html += "<div class='progress-bg'><div class='progress-fill' style='width:"+std::to_string(pct)+"%; background:"+col+"'></div></div>";
            
            if (pct > 50.0) {
                double max_days = (double)u.max_threshold / (double)DAY_SEC;
                std::stringstream ss_max;
                ss_max << std::fixed << std::setprecision(1) << max_days;
                html += "<div style='text-align:center; font-size:0.75em; color:#ff5252; margin-top:-12px; margin-bottom:15px; opacity:0.8; letter-spacing:0.5px;'>⚠ BANKRUPTCY LIMIT: " + ss_max.str() + " DAYS</div>";
            }
            
            html += "<div class='btn-grid'>";
            html += "<a href='/virtue/1?name="+u.id+"'><button class='btn virtue1-btn'>" + u.virtue1_name + " (-1d)</button></a>";
            html += "<a href='/virtue/2?name="+u.id+"'><button class='btn virtue2-btn'>" + u.virtue2_name + " (-1d)</button></a>";
            html += "</div>";
            
            double days_d = (double)u.base_cost / (double)DAY_SEC;
            if (u.debt_seconds > 0) days_d = days_d * 1.5;
            
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << days_d;
            
            html += "<a href='/vice?name="+u.id+"'><button class='btn smoke-btn'>Indulge (+" + ss.str() + "d)</button></a>";
            
            html += "<details><summary>View Weekly Insights</summary>";
            html += render_calendar(u.name); 
            html += "</details>";
        }
        html += "</div>";
    }

    // 2. TRANSACTIONS
    html += render_feed();
    
    // Undo Link
    if (!activity_feed.empty() && activity_feed.front().user_name == users[current_user_id].name) {
        time_t now = std::time(nullptr);
        if (std::difftime(now, activity_feed.front().timestamp) < 600) {
            html += "<div style='text-align:center; margin-top:5px;'><a href='/undo' style='color:#666; font-size:0.8em; text-decoration:none;'>⎌ Undo Last Action</a></div>";
        }
    }

    // 3. HOUSEHOLD
    html += "<div class='household-grid'>";
    for (auto& [key, u] : users) {
        if (key == current_user_id) continue;
        
        std::string accent = get_user_color(u.name);
        
        html += "<div class='mini-card " + std::string(u.locked?"locked":"") + "' style='--accent:" + accent + "'>";
        html += "<div class='tag'><span>" + u.name + "</span> <span class='streak'>🔥 " + std::to_string(u.streak) + "</span></div>";
        
        if (u.locked) {
            html += "<div class='mini-timer' style='color:#ff5252'>BANKRUPT</div>";
            html += "<a href='/reset?name="+u.id+"'><button class='btn punishment-btn'>Bail Out</button></a>";
        } else {
            html += "<div class='mini-timer' data-seconds='" + std::to_string(u.debt_seconds) + "'>...</div>";
            html += "<div style='font-size:0.7em; color:#666'>Target: " + u.virtue1_name + " & " + u.virtue2_name + "</div>";
        }
        html += "</div>";
    }
    html += "</div>";

    html += "<div class='logout'><a href='/logout'>Log Out</a></div>";
    html += "</body></html>";
    return html;
}

// --- ROUTES ---

int main() {
    load_db();
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](const crow::request& req){
        std::string user_id = get_logged_in_user(req);
        if (user_id == "") {
            crow::response res(302);
            res.add_header("Location", "/login");
            return res;
        }
        return crow::response(render_dashboard(user_id));
    });

    CROW_ROUTE(app, "/login")([](const crow::request& req){
        std::string err = req.url_params.get("error") ? req.url_params.get("error") : "";
        std::string msg = (err == "invalid") ? "Invalid Credentials" : "";
        return render_login(msg);
    });

    CROW_ROUTE(app, "/signup")([](const crow::request& req){
        std::string err = req.url_params.get("error") ? req.url_params.get("error") : "";
        std::string msg = (err == "exists") ? "Name taken" : "";
        return render_signup_wizard(msg);
    });

    CROW_ROUTE(app, "/edit")([](const crow::request& req){
        std::string user_id = get_logged_in_user(req);
        if (user_id == "" || users.find(user_id) == users.end()) {
             crow::response res(302);
             res.add_header("Location", "/login");
             return res;
        }
        return crow::response(render_edit_page(users[user_id]));
    });

    CROW_ROUTE(app, "/edit").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        std::string name = get_logged_in_user(req);
        if (name == "" || users.find(name) == users.end()) {
             crow::response res(302);
             res.add_header("Location", "/login");
             return res;
        }
        
        std::string vice = get_form_value(req.body, "vice");
        std::string new_pass = get_form_value(req.body, "new_password");
        
        double vice_freq = std::stod(get_form_value(req.body, "vice_freq"));
        double vice_per = std::stod(get_form_value(req.body, "vice_per"));
        double days_interval = vice_per / vice_freq;

        std::string v1n = get_form_value(req.body, "v1name");
        double v1_freq = std::stod(get_form_value(req.body, "v1_freq"));
        double v1_per = std::stod(get_form_value(req.body, "v1_per"));
        double v1_weekly = (v1_freq / v1_per) * 7.0;

        std::string v2n = get_form_value(req.body, "v2name");
        double v2_freq = std::stod(get_form_value(req.body, "v2_freq"));
        double v2_per = std::stod(get_form_value(req.body, "v2_per"));
        double v2_weekly = (v2_freq / v2_per) * 7.0;

        try {
            User& u = users[name];
            
            // NEW: Update Password if provided
            if (!new_pass.empty()) {
                u.password = new_pass;
            }

            u.vice = vice;
            u.target_interval_days = days_interval;
            u.virtue1_name = v1n;
            u.promised_v1_weekly = v1_weekly;
            u.virtue2_name = v2n;
            u.promised_v2_weekly = v2_weekly;
            
            u.calculate_math();
            save_db();
        } catch (...) {}
        
        crow::response res(302);
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/undo")([](const crow::request& req){
        std::string cur_id = get_logged_in_user(req);
        if (!cur_id.empty() && users.count(cur_id)) {
            if (perform_undo(users[cur_id])) {
                // Success
            }
        }
        crow::response res(302);
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        std::string name = get_form_value(req.body, "name");
        std::string pass = get_form_value(req.body, "password");
        
        std::string id = name;
        std::transform(id.begin(), id.end(), id.begin(), ::tolower);

        crow::response res(302);
        if (users.count(id) && users[id].password == pass) {
            res.add_header("Set-Cookie", "user=" + id + "; Path=/; HttpOnly; Max-Age=31536000");
            res.add_header("Location", "/");
        } else {
            res.add_header("Location", "/login?error=invalid");
        }
        return res;
    });

    CROW_ROUTE(app, "/signup").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        std::string name = get_form_value(req.body, "name");
        std::string pass = get_form_value(req.body, "password");
        std::string vice = get_form_value(req.body, "vice");
        
        double vice_freq = std::stod(get_form_value(req.body, "vice_freq"));
        double vice_per = std::stod(get_form_value(req.body, "vice_per"));
        double days_interval = vice_per / vice_freq;

        std::string v1n = get_form_value(req.body, "v1name");
        double v1_freq = std::stod(get_form_value(req.body, "v1_freq"));
        double v1_per = std::stod(get_form_value(req.body, "v1_per"));
        double v1_weekly = (v1_freq / v1_per) * 7.0;

        std::string v2n = get_form_value(req.body, "v2name");
        double v2_freq = std::stod(get_form_value(req.body, "v2_freq"));
        double v2_per = std::stod(get_form_value(req.body, "v2_per"));
        double v2_weekly = (v2_freq / v2_per) * 7.0;

        std::string id = name;
        std::transform(id.begin(), id.end(), id.begin(), ::tolower);

        crow::response res(302);
        if (users.count(id)) {
            res.add_header("Location", "/signup?error=exists");
            return res;
        }

        users[id] = User(name, pass, vice, days_interval, v1n, v1_weekly, v2n, v2_weekly);
        save_db();
        res.add_header("Set-Cookie", "user=" + id + "; Path=/; HttpOnly; Max-Age=31536000");
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/logout")([](){
        crow::response res(302);
        res.add_header("Set-Cookie", "user=; Path=/; Max-Age=0");
        res.add_header("Location", "/login");
        return res;
    });

    CROW_ROUTE(app, "/vice")([](const crow::request& req){
        auto name = req.url_params.get("name");
        std::string cur_id = get_logged_in_user(req);
        if (name && cur_id == name) add_vice(users[name]);
        crow::response res(302);
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/virtue/1")([](const crow::request& req){
        auto name = req.url_params.get("name");
        std::string cur_id = get_logged_in_user(req);
        if (name && cur_id == name) perform_virtue(users[name], 1);
        crow::response res(302);
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/virtue/2")([](const crow::request& req){
        auto name = req.url_params.get("name");
        std::string cur_id = get_logged_in_user(req);
        if (name && cur_id == name) perform_virtue(users[name], 2);
        crow::response res(302);
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/reset")([](const crow::request& req){
        auto target_id = req.url_params.get("name");
        std::string cur_id = get_logged_in_user(req);
        if (target_id && !cur_id.empty() && users.count(target_id) && cur_id != std::string(target_id)) {
            reset_user(users[target_id], users[cur_id].name);
        }
        crow::response res(302);
        res.add_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/delete_account").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        std::string name = get_logged_in_user(req);
        if (name != "" && users.count(name)) {
            // 1. Remove User
            users.erase(name);
            
            // 2. Cleanup Logs (Optional: Remove logs belonging to this user)
            // We use a new deque to filter out the deleted user's logs
            std::deque<ActivityLog> new_feed;
            for (const auto& log : activity_feed) {
                if (log.user_name != name) { // Note: log.user_name stores the Display Name, 'name' here is ID
                    // This creates a slight mismatch issue if Display Name != ID.
                    // To be safe, we just keep the logs for history, or strictly check IDs if stored.
                    // For this MVP, let's just keep the logs or simple filter:
                    new_feed.push_back(log); 
                }
            }
            // If you want to wipe their history, uncomment:
            // activity_feed = new_feed; 
            
            save_db();
        }
        
        crow::response res(302);
        res.add_header("Set-Cookie", "user=; Path=/; Max-Age=0"); // Clear Cookie
        res.add_header("Location", "/login");
        return res;
    });
    
    app.port(18080).multithreaded().run();
}