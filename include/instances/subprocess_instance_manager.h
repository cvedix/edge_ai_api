#pragma once

#include "instances/instance_manager.h"
#include "instances/instance_storage.h"
#include "solutions/solution_registry.h"
#include "worker/worker_supervisor.h"
#include <memory>
#include <mutex>
#include <unordered_map>

/**
 * @brief Subprocess-based Instance Manager
 *
 * Implements IInstanceManager using WorkerSupervisor for subprocess isolation.
 * Each instance runs in its own worker process, providing:
 * - Memory isolation (leaks don't affect main server)
 * - Crash isolation (one instance crash doesn't affect others)
 * - Hot reload capability (restart worker without restarting API)
 */
class SubprocessInstanceManager : public IInstanceManager {
public:
  /**
   * @brief Constructor
   * @param solutionRegistry Reference to solution registry
   * @param instanceStorage Reference to instance storage
   * @param workerExecutable Path to worker executable (default:
   * "edge_ai_worker")
   */
  SubprocessInstanceManager(
      SolutionRegistry &solutionRegistry, InstanceStorage &instanceStorage,
      const std::string &workerExecutable = "edge_ai_worker");

  ~SubprocessInstanceManager() override;

  // ========== Instance Lifecycle ==========

  std::string createInstance(const CreateInstanceRequest &req) override;
  bool deleteInstance(const std::string &instanceId) override;
  bool startInstance(const std::string &instanceId,
                     bool skipAutoStop = false) override;
  bool stopInstance(const std::string &instanceId) override;
  bool updateInstance(const std::string &instanceId,
                      const Json::Value &configJson) override;

  // ========== Instance Query ==========

  std::optional<InstanceInfo>
  getInstance(const std::string &instanceId) const override;
  std::vector<std::string> listInstances() const override;
  std::vector<InstanceInfo> getAllInstances() const override;
  bool hasInstance(const std::string &instanceId) const override;
  int getInstanceCount() const override;

  // ========== Instance Data ==========

  std::optional<InstanceStatistics>
  getInstanceStatistics(const std::string &instanceId) override;
  std::string getLastFrame(const std::string &instanceId) const override;
  Json::Value getInstanceConfig(const std::string &instanceId) const override;
  bool updateInstanceFromConfig(const std::string &instanceId,
                                const Json::Value &configJson) override;
  bool hasRTMPOutput(const std::string &instanceId) const override;

  // ========== Instance Management Operations ==========

  void loadPersistentInstances() override;
  int checkAndHandleRetryLimits() override;

  // ========== Backend Info ==========

  std::string getBackendType() const override { return "subprocess"; }
  bool isSubprocessMode() const override { return true; }

  // ========== Subprocess-specific ==========

  /**
   * @brief Get worker supervisor (for advanced operations)
   */
  worker::WorkerSupervisor &getSupervisor() { return *supervisor_; }

  /**
   * @brief Stop all workers gracefully
   */
  void stopAllWorkers();

private:
  SolutionRegistry &solution_registry_;
  InstanceStorage &instance_storage_;
  std::unique_ptr<worker::WorkerSupervisor> supervisor_;

  // Local cache of instance info (synced with workers)
  mutable std::mutex instances_mutex_;
  std::unordered_map<std::string, InstanceInfo> instances_;

  /**
   * @brief Build config JSON from CreateInstanceRequest
   */
  Json::Value buildWorkerConfig(const CreateInstanceRequest &req) const;

  /**
   * @brief Update local instance cache from worker response
   */
  void updateInstanceCache(const std::string &instanceId,
                           const worker::IPCMessage &response);

  /**
   * @brief Handle worker state changes
   */
  void onWorkerStateChange(const std::string &instanceId,
                           worker::WorkerState oldState,
                           worker::WorkerState newState);

  /**
   * @brief Handle worker errors
   */
  void onWorkerError(const std::string &instanceId, const std::string &error);
};
