#pragma once
#include <IControls.h>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "NeuralAmpModelerCore/dsp/wav.h"


using namespace iplug;
using namespace igraphics;

class FolderBrowser
{
private:
  inline static IGraphics* lGraphics;
  inline static int l_iCurrentNAMIndex = 0;
  inline static int l_iCurrentIRIndex = 0;
  inline static std::vector<std::string> _oNAMs;
  inline static std::string _oCurrentNAM;
  inline static std::vector<std::string> _oIRs;
  inline static std::string _oCurrentIR;

public:
  FolderBrowser::FolderBrowser(IGraphics* pGraphics);

  void InitializeNAMNav(WDL_String fileName);

  WDL_String StartNAMNavUp();

  int FinishNAMNavUp(std::string l_sMsg);

  WDL_String StartNAMNavDown();

  int FinishNAMNavDown(std::string l_sMsg);

  void ShowNAMArrows();

  void HideNAMArrows();

  void InitializeIRNav(WDL_String fileName);

  WDL_String FolderBrowser::StartIRNavUp();

  int FolderBrowser::FinishIRNavUp(dsp::wav::LoadReturnCode l_sMsg);

  WDL_String FolderBrowser::StartIRNavDown();

  int FolderBrowser::FinishIRNavDown(dsp::wav::LoadReturnCode l_sMsg);

  void ShowIRArrows();

  void HideIRArrows();
};
