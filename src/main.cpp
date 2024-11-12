#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

constexpr int NUM_PLAYERS = 4;
std::counting_semaphore<NUM_PLAYERS> chair_semaphore(NUM_PLAYERS - 1);
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> is_music_stopped{false};
int chair_counter = 1;

class MusicalChairsGame {
public:
    MusicalChairsGame(int players)
        : available_chairs(players - 1) {}

    void start_round(int active_players) {
        available_chairs--;
        chair_counter = 1;

        while (chair_semaphore.try_acquire())
            ;
        chair_semaphore.release(available_chairs);
        is_music_stopped = false;

        if (active_players > 1) {
            std::cout << "\nStarting round with " << active_players 
                      << " players and " << available_chairs << " chairs.\n";
            std::cout << "Music is playing... 🎵\n\n";
        }
    }

    void stop_music() {
        std::unique_lock<std::mutex> lock(music_mutex);
        is_music_stopped = true;
        music_cv.notify_all();
        std::cout << "> Music stopped! Players are rushing to sit...\n\n";
    }

    bool is_game_on(int active_players) const {
        return active_players > 1;
    }

private:
    int available_chairs;
};

class Player {
public:
    Player(int id)
        : id(id), is_active(true), has_tried(false) {}

    bool is_active_player() const { return is_active; }
    int get_id() const { return id; }

    void try_to_sit() {
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock, [] { return is_music_stopped.load(); });

        if (is_active && !has_tried) {
            has_tried = true;
            if (chair_semaphore.try_acquire()) {
                std::cout << "[Chair " << chair_counter++ << "] Taken by P" << id << "\n";
            } else {
                is_active = false;
                std::cout << "\nPlayer P" << id << " missed a chair and is eliminated!\n";
            }
        }
    }

    void reset_round() { has_tried = false; }

    void play() {
        while (is_active) {
            try_to_sit();
            if (!is_active)
                std::this_thread::yield();
        }
    }

private:
    int id;
    bool is_active;
    bool has_tried;
};

class Coordinator {
public:
    Coordinator(MusicalChairsGame &game, std::vector<Player> &players)
        : game(game), players(players) {}

    void manage_game() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> duration(1000, 3000);

        while (game.is_game_on(count_active_players())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(duration(gen)));
            game.stop_music();

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            release_unused_chairs();
            game.start_round(count_active_players());
            reset_players_for_round();
        }

        announce_winner();
    }

private:
    MusicalChairsGame &game;
    std::vector<Player> &players;

    int count_active_players() const {
        int count = 0;
        for (const auto &player : players) {
            if (player.is_active_player())
                count++;
        }
        return count;
    }

    int find_winner() const {
        for (const auto &player : players) {
            if (player.is_active_player())
                return player.get_id();
        }
        return -1;
    }

    void release_unused_chairs() {
        chair_semaphore.release(NUM_PLAYERS - 1);
    }

    void reset_players_for_round() {
        for (auto &player : players) {
            player.reset_round();
        }
    }

    void announce_winner() const {
        int winner_id = find_winner();
        std::cout << "\n🏆 Winner: Player P" << winner_id << "! Congratulations! 🏆\n";
    }
};

int main() {
    MusicalChairsGame game(NUM_PLAYERS);
    std::vector<Player> players;
    for (int i = 1; i <= NUM_PLAYERS; ++i) {
        players.emplace_back(i);
    }

    Coordinator coordinator(game, players);
    std::vector<std::thread> player_threads;

    for (auto &player : players) {
        player_threads.emplace_back(&Player::play, &player);
    }

    std::thread game_thread(&Coordinator::manage_game, &coordinator);

    for (auto &t : player_threads) {
        if (t.joinable())
            t.join();
    }

    if (game_thread.joinable())
        game_thread.join();

    std::cout << "Musical Chairs Game Over.\n";
    return 0;
}
