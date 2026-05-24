#pragma once
#include "common/sample_app.h"

class TriangleApp : public ISampleApp {
 public:
  virtual void OnInitialize() override;
  virtual void OnDrawFrame() override;
  virtual void OnCleanup() override;
};
