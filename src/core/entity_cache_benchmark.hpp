#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <cstdint>
#include "optimized_entity_cache.hpp"

namespace core
{
    // === Entity Cache Performance Benchmark ===
    // Purpose: Validate performance improvements and compare old vs new systems
    class EntityCacheBenchmark
    {
    private:
        struct BenchmarkResult
        {
            std::string test_name;
            uint64_t duration_ms = 0;
            uint64_t rpm_calls = 0;
            uint64_t entities_found = 0;
            double rpm_calls_per_second = 0.0;
            bool success = false;
            std::string notes;
        };
        
        std::vector<BenchmarkResult> m_results;
        
    public:
        // === Benchmark Tests ===
        
        // Test 1: Old System Full Scan
        BenchmarkResult benchmark_old_system()
        {
            BenchmarkResult result;
            result.test_name = "Old System (Full 0-4096 Scan)";
            
            auto start = std::chrono::high_resolution_clock::now();
            uint64_t rpm_calls = 0;
            uint64_t entities_found = 0;
            
            // Simulate old system: scan 0-4096 every frame
            for (int32_t idx = 0; idx < 4096; idx++) {
                const auto entity = i::m_game_entity_system->get(idx);
                rpm_calls++;
                
                if (entity && entity->get_ref_e_handle().is_valid()) {
                    entities_found++;
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            result.rpm_calls = rpm_calls;
            result.entities_found = entities_found;
            result.rpm_calls_per_second = (static_cast<double>(rpm_calls) / result.duration_ms) * 1000.0;
            result.success = true;
            result.notes = "Full entity scan - baseline performance";
            
            return result;
        }
        
        // Test 2: New Optimized System
        BenchmarkResult benchmark_optimized_system()
        {
            BenchmarkResult result;
            result.test_name = "Optimized System (Bitmap-based)";
            
            auto start = std::chrono::high_resolution_clock::now();
            
            // Warm up the cache
            core::g_optimized_entity_cache.update_optimized();
            
            // Get performance stats
            auto stats = core::g_optimized_entity_cache.get_performance_stats();
            
            auto end = std::chrono::high_resolution_clock::now();
            result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            result.rpm_calls = stats.rpm_calls_saved; // RPM calls made by optimized system
            result.entities_found = stats.total_cached;
            result.rpm_calls_per_second = (static_cast<double>(result.rpm_calls) / result.duration_ms) * 1000.0;
            result.success = true;
            result.notes = "Bitmap-based scanning with optimizations";
            
            return result;
        }
        
        // Test 3: Player Retrieval Performance
        BenchmarkResult benchmark_player_retrieval()
        {
            BenchmarkResult result;
            result.test_name = "Player Retrieval Performance";
            
            auto start = std::chrono::high_resolution_clock::now();
            
            // Test player retrieval 1000 times
            constexpr int iterations = 1000;
            uint64_t total_players = 0;
            
            for (int i = 0; i < iterations; i++) {
                auto players = core::g_optimized_entity_cache.get_player_indices_fast();
                total_players += players.size();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            result.rpm_calls = 0; // No RPM calls for cached retrieval
            result.entities_found = total_players / iterations; // Average players found
            result.rpm_calls_per_second = 0.0;
            result.success = true;
            result.notes = "Fast player retrieval using bitmap";
            
            return result;
        }
        
        // Test 4: Memory Usage Analysis
        BenchmarkResult benchmark_memory_usage()
        {
            BenchmarkResult result;
            result.test_name = "Memory Usage Analysis";
            
            auto start = std::chrono::high_resolution_clock::now();
            
            auto stats = core::g_optimized_entity_cache.get_performance_stats();
            
            // Calculate memory usage
            size_t cache_memory = stats.total_cached * sizeof(core::OptimizedCachedEntity);
            size_t bitmap_memory = 4 * 16 * sizeof(uint64_t); // 4 bitmaps * 16 blocks
            size_t generation_memory = 1024 * sizeof(uint32_t);
            size_t total_memory = cache_memory + bitmap_memory + generation_memory;
            
            auto end = std::chrono::high_resolution_clock::now();
            result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            result.rpm_calls = 0;
            result.entities_found = total_memory;
            result.rpm_calls_per_second = 0.0;
            result.success = true;
            result.notes = "Memory usage: " + std::to_string(total_memory / 1024) + " KB total";
            
            return result;
        }
        
        // === Run Complete Benchmark ===
        void run_complete_benchmark()
        {
            printf("\n=== ENTITY CACHE PERFORMANCE BENCHMARK ===\n");
            printf("Starting comprehensive performance testing...\n\n");
            
            m_results.clear();
            
            // Test 1: Old System
            printf("Running Test 1: Old System Full Scan...\n");
            auto old_result = benchmark_old_system();
            m_results.push_back(old_result);
            printf("  Duration: %llu ms\n", old_result.duration_ms);
            printf("  RPM Calls: %llu\n", old_result.rpm_calls);
            printf("  Entities Found: %llu\n", old_result.entities_found);
            printf("  RPM/sec: %.0f\n", old_result.rpm_calls_per_second);
            printf("  Notes: %s\n\n", old_result.notes.c_str());
            
            // Test 2: Optimized System
            printf("Running Test 2: Optimized System...\n");
            auto optimized_result = benchmark_optimized_system();
            m_results.push_back(optimized_result);
            printf("  Duration: %llu ms\n", optimized_result.duration_ms);
            printf("  RPM Calls: %llu\n", optimized_result.rpm_calls);
            printf("  Entities Found: %llu\n", optimized_result.entities_found);
            printf("  RPM/sec: %.0f\n", optimized_result.rpm_calls_per_second);
            printf("  Notes: %s\n\n", optimized_result.notes.c_str());
            
            // Test 3: Player Retrieval
            printf("Running Test 3: Player Retrieval Performance...\n");
            auto player_result = benchmark_player_retrieval();
            m_results.push_back(player_result);
            printf("  Duration: %llu ms\n", player_result.duration_ms);
            printf("  Average Players: %llu\n", player_result.entities_found);
            printf("  Notes: %s\n\n", player_result.notes.c_str());
            
            // Test 4: Memory Usage
            printf("Running Test 4: Memory Usage Analysis...\n");
            auto memory_result = benchmark_memory_usage();
            m_results.push_back(memory_result);
            printf("  Duration: %llu ms\n", memory_result.duration_ms);
            printf("  Memory Usage: %llu bytes\n", memory_result.entities_found);
            printf("  Notes: %s\n\n", memory_result.notes.c_str());
            
            // Generate comparison report
            generate_comparison_report();
        }
        
    private:
        void generate_comparison_report()
        {
            printf("=== PERFORMANCE COMPARISON REPORT ===\n");
            
            if (m_results.size() < 2) {
                printf("❌ Insufficient data for comparison\n");
                return;
            }
            
            const auto& old_result = m_results[0];
            const auto& optimized_result = m_results[1];
            
            // Calculate improvements
            double rpm_reduction = ((static_cast<double>(old_result.rpm_calls) - optimized_result.rpm_calls) / old_result.rpm_calls) * 100.0;
            double speed_improvement = ((static_cast<double>(old_result.duration_ms) - optimized_result.duration_ms) / old_result.duration_ms) * 100.0;
            
            printf("\n--- Key Metrics ---\n");
            printf("RPM Call Reduction: %.1f%%\n", rpm_reduction);
            printf("Speed Improvement: %.1f%%\n", speed_improvement);
            
            // Performance rating
            printf("\n--- Performance Rating ---\n");
            if (rpm_reduction > 70.0) {
                printf("🏆 EXCELLENT: Significant performance improvements achieved!\n");
            } else if (rpm_reduction > 50.0) {
                printf("✅ GOOD: Notable performance improvements.\n");
            } else if (rpm_reduction > 25.0) {
                printf("⚠️ MODERATE: Some performance gains.\n");
            } else {
                printf("❌ POOR: Minimal performance improvements.\n");
            }
            
            // Recommendations
            printf("\n--- Recommendations ---\n");
            if (rpm_reduction < 50.0) {
                printf("• Consider further bitmap optimizations\n");
                printf("• Review entity classification efficiency\n");
            }
            
            if (optimized_result.entities_found < old_result.entities_found) {
                printf("• Check entity detection accuracy\n");
            }
            
            printf("• Monitor real-world performance in-game\n");
            printf("• Consider adaptive scanning intervals\n");
            
            printf("\n=====================================\n");
        }
        
    public:
        // Quick performance check (for runtime monitoring)
        void quick_performance_check()
        {
            auto stats = core::g_optimized_entity_cache.get_performance_stats();
            
            printf("\n[QUICK CHECK] Entity Cache Performance:\n");
            printf("  Cached Entities: %zu\n", stats.total_cached);
            printf("  Players: %zu\n", stats.player_count);
            printf("  Weapons: %zu\n", stats.weapon_count);
            printf("  RPM Reduction: %.1f%%\n", stats.rpm_reduction);
            printf("  RPM Calls Saved/sec: %llu\n", stats.rpm_calls_saved);
            
            // Performance status
            if (stats.rpm_reduction > 70.0) {
                printf("  Status: 🏆 OPTIMAL\n");
            } else if (stats.rpm_reduction > 50.0) {
                printf("  Status: ✅ GOOD\n");
            } else {
                printf("  Status: ⚠️ NEEDS IMPROVEMENT\n");
            }
            printf("\n");
        }
    };
    
    // Global benchmark instance
    inline EntityCacheBenchmark g_entity_cache_benchmark;
}
