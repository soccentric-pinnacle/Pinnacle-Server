// src/main.cpp — Pinnacle Server entry point.
// Starts gRPC + Drogon (HTTP/REST) servers after running DB migrations.

#include "config/Settings.h"
#include "logging/LoggingSetup.h"
#include "db/DbClient.h"
#include "db/MigrationRunner.h"
#include "registry/ModuleRegistry.h"

#include "services/AuthService.h"
#include "services/RunService.h"
#include "services/LogService.h"
#include "services/ModuleService.h"
#include "services/TargetService.h"
#include "services/ConfigService.h"
#include "services/TemplateService.h"
#include "services/ScheduleService.h"
#include "services/OtaService.h"
#include "services/NotificationService.h"
#include "services/ThresholdService.h"
#include "services/AnomalyService.h"
#include "services/CompareService.h"

#include "grpc_server/HalServiceImpl.h"
#include "grpc_server/ClientServiceImpl.h"
#include "grpc_server/GrpcServerRunner.h"

#include "rest_api/DrogonAppFactory.h"

#include "orchestration/RunManager.h"
#include "orchestration/Scheduler.h"
#include "orchestration/TargetHeartbeat.h"
#include "orchestration/OtaOrchestrator.h"
#include "orchestration/RetentionScheduler.h"

#include <spdlog/spdlog.h>
#include <drogon/drogon.h>

#include <csignal>
#include <atomic>
#include <thread>
#include <filesystem>

// ------------------------------------------------------------------
// Signal handling
// ------------------------------------------------------------------
namespace {
std::atomic<bool> g_shutdown{false};

void handle_signal(int /*sig*/) {
    g_shutdown = true;
    drogon::app().quit();
}
}  // namespace

int main() {
    // --- Load settings from environment ---
    pinnacle::Settings settings;
    try {
        settings = pinnacle::load_settings();
    } catch (const std::exception& e) {
        spdlog::critical("Failed to load settings: {}", e.what());
        return 1;
    }

    // --- Logging ---
    pinnacle::logging::setup_logging(settings.log_level);
    spdlog::info("Pinnacle Server starting — {}", pinnacle::settings_summary(settings));

    // --- Database ---
    try {
        pinnacle::db::init_db_client(settings.db);
    } catch (const std::exception& e) {
        spdlog::critical("DB init failed: {}", e.what());
        return 1;
    }

    // Run migrations.
    pinnacle::db::MigrationRunner migrations{PINNACLE_MIGRATIONS_DIR};
    try {
        migrations.run();
    } catch (const std::exception& e) {
        spdlog::critical("Migration failed: {}", e.what());
        return 1;
    }

    // --- Module Registry ---
    pinnacle::ModuleRegistry registry;
    if (!settings.registry_path.empty() &&
        std::filesystem::exists(settings.registry_path)) {
        registry.load(settings.registry_path);
        spdlog::info("Module registry loaded: {} modules", registry.size());
    } else {
        spdlog::warn("No module registry found at '{}'", settings.registry_path);
    }

    // --- Services ---
    pinnacle::AuthService          auth_svc{settings.auth};
    pinnacle::RunService           run_svc;
    pinnacle::LogService           log_svc;
    pinnacle::ModuleService        mod_svc{registry};
    pinnacle::TargetService        target_svc;
    pinnacle::ConfigService        config_svc;
    pinnacle::TemplateService      template_svc;
    pinnacle::ScheduleService      schedule_svc;
    pinnacle::OtaService           ota_svc{settings.ota};
    pinnacle::NotificationService  notif_svc{settings.smtp};
    pinnacle::ThresholdService     threshold_svc;
    pinnacle::AnomalyService       anomaly_svc;
    pinnacle::CompareService       compare_svc;

    // --- Orchestration ---
    pinnacle::orchestration::RunManager      run_mgr{run_svc, log_svc, notif_svc};
    pinnacle::orchestration::Scheduler       scheduler{schedule_svc, run_svc};
    pinnacle::orchestration::TargetHeartbeat heartbeat{target_svc};
    pinnacle::orchestration::OtaOrchestrator ota_orch{ota_svc};
    pinnacle::RetentionScheduler             retention{run_svc};

    // Reconcile any runs that were left in 'running'/'queued' state from a previous crash.
    {
        int stale = run_svc.reconcile_stale_runs();
        if (stale > 0) {
            spdlog::warn("Reconciled {} stale run(s) to 'failed' on startup", stale);
        }
    }

    scheduler.start();
    heartbeat.start();
    ota_orch.start();
    retention.start();

    // --- gRPC Server (in background thread) ---
    pinnacle::grpc::HalServiceImpl    hal_service{run_svc, log_svc, mod_svc, settings};
    pinnacle::grpc::ClientServiceImpl client_service{run_svc, log_svc};
    pinnacle::grpc::GrpcServerRunner  grpc_runner{hal_service, client_service, settings.network};

    std::thread grpc_thread([&] {
        try {
            grpc_runner.start();
        } catch (const std::exception& e) {
            spdlog::critical("gRPC server error: {}", e.what());
            g_shutdown = true;
            drogon::app().quit();
        }
    });

    // --- Signals ---
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    // --- REST API (Drogon — blocks until quit()) ---
    pinnacle::rest::configure_drogon_app(settings,
                                        auth_svc,
                                        run_svc,
                                        log_svc,
                                        mod_svc,
                                        target_svc,
                                        config_svc,
                                        template_svc,
                                        schedule_svc,
                                        ota_svc,
                                        notif_svc,
                                        compare_svc);

    spdlog::info("Starting Drogon HTTP server on {}", settings.network.http_listen);
    drogon::app().run();  // blocks

    // --- Shutdown ---
    spdlog::info("Shutting down...");
    scheduler.stop();
    heartbeat.stop();
    retention.stop();
    ota_orch.stop();
    grpc_runner.shutdown();
    if (grpc_thread.joinable()) grpc_thread.join();

    spdlog::info("Pinnacle Server stopped");
    return 0;
}
