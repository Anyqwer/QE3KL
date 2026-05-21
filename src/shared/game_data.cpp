#include "pch.hpp"
#include "game_data.hpp"
#include <chrono>

namespace shared
{
    void GameState::update_players(const std::vector<PlayerData>& new_players)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        players = new_players;
    }

    void GameState::update_bomb(const BombData& new_bomb)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        bomb = new_bomb;
    }

    void GameState::update_local(const LocalPlayerData& new_local)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        local = new_local;
    }

    void GameState::update_map(const std::string& new_map_name)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        map_name = new_map_name;
    }

    void GameState::clear()
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        players.clear();
        bomb = BombData{};
        local = LocalPlayerData{};
        map_name.clear();
    }

    std::vector<PlayerData> GameState::get_players() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return players;  // Copy - safe to use without lock
    }

    BombData GameState::get_bomb() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return bomb;
    }

    LocalPlayerData GameState::get_local() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return local;
    }

    std::string GameState::get_map_name() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return map_name;
    }

    // Double-buffered implementation with proper memory ordering
    void DoubleBufferedGameState::write_players(const std::vector<PlayerData>& players)
    {
        // Relaxed is sufficient for getting write index (only this thread writes to it)
        int idx = write_index.load(std::memory_order_relaxed);
        buffers[idx].update_players(players);
        // All writes to buffer happen-before swap (enforced by swap_buffers memory barrier)
    }

    void DoubleBufferedGameState::write_bomb(const BombData& bomb)
    {
        int idx = write_index.load(std::memory_order_relaxed);
        buffers[idx].update_bomb(bomb);
    }

    void DoubleBufferedGameState::write_local(const LocalPlayerData& local)
    {
        int idx = write_index.load(std::memory_order_relaxed);
        buffers[idx].update_local(local);
    }

    void DoubleBufferedGameState::write_map(const std::string& map_name)
    {
        int idx = write_index.load(std::memory_order_relaxed);
        buffers[idx].update_map(map_name);
    }

    void DoubleBufferedGameState::write_clear()
    {
        int idx = write_index.load(std::memory_order_relaxed);
        buffers[idx].clear();
    }

    std::vector<PlayerData> DoubleBufferedGameState::read_players() const
    {
        // Acquire: ensure we see all writes that happened before the last swap
        int idx = read_index.load(std::memory_order_acquire);
        return buffers[idx].get_players();
    }

    BombData DoubleBufferedGameState::read_bomb() const
    {
        int idx = read_index.load(std::memory_order_acquire);
        return buffers[idx].get_bomb();
    }

    LocalPlayerData DoubleBufferedGameState::read_local() const
    {
        int idx = read_index.load(std::memory_order_acquire);
        return buffers[idx].get_local();
    }

    std::string DoubleBufferedGameState::read_map() const
    {
        int idx = read_index.load(std::memory_order_acquire);
        return buffers[idx].get_map_name();
    }

    uint64_t DoubleBufferedGameState::read_last_update_time() const
    {
        return last_update_time.load(std::memory_order_acquire);
    }

    void DoubleBufferedGameState::swap_buffers()
    {
        // Memory barrier: ensure all writes to write buffer are visible before swap
        int current_write = write_index.load(std::memory_order_acquire);
        int current_read = read_index.load(std::memory_order_acquire);
        
        // Update timestamp before swap (with release semantics)
        auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        last_update_time.store(current_time, std::memory_order_release);
        
        // Swap indices with sequential consistency (full barrier)
        // This ensures: 1) no reordering around swap, 2) visibility across all threads
        write_index.store(current_read, std::memory_order_seq_cst);
        read_index.store(current_write, std::memory_order_seq_cst);
    }
}
