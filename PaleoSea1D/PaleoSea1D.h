#pragma once

// Plugin implementation class
class CPaleosea1d
{
public:
  CPaleosea1d(IfmDocument pDoc);
  ~CPaleosea1d();
  static CPaleosea1d* FromHandle(IfmDocument pDoc);

#pragma region IFM_Definitions
  // Implementation
public:
  void PreSimulation (IfmDocument pDoc);
  void PostSimulation (IfmDocument pDoc);
  void PreTimeStep (IfmDocument pDoc);
  void PostTimeStep (IfmDocument pDoc);
#pragma endregion

private:
  IfmDocument m_pDoc;
};
