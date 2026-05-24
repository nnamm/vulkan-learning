#pragma once

class ISampleApp {
 public:
  virtual ~ISampleApp() = default;
  virtual void OnInitialize() = 0;
  virtual void OnDrawFrame() = 0;
  virtual void OnCleanup() = 0;
};

// Checker Macro
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#error need to define `GLM_FORCE_DEPTH_ZERO_TO_ONE`
#endif
