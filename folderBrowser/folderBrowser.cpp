#include "folderBrowser.h"

FolderBrowser::FolderBrowser(IGraphics *pGraphics) { lGraphics = pGraphics; }

void FolderBrowser::InitializeNAMNav(WDL_String fileName) {
  _oCurrentNAM = fileName.Get();
  _oNAMs.clear();

  // get all .nams in folder of currently selected NAM
  const std::filesystem::path l_sPath =
      std::filesystem::current_path().u8string();
  std::set<std::filesystem::path> sorted_by_name; // sort listing

  for (const auto &p : std::filesystem::directory_iterator(l_sPath)) {
    if (!std::filesystem::is_directory(p)) {
      if (std::filesystem::path(p.path()).extension() == ".nam") {
        _oNAMs.push_back(p.path().u8string());
      }
    }
  }

  // set current index for selected NAM
  for (int i = 0; i < _oNAMs.size(); i++) {
    if (_oNAMs[i] == _oCurrentNAM) {
      l_iCurrentNAMIndex = i;
      break;
    }
  }

  if (_oNAMs.size() > 1) {
    // more than one NAM in current folder, show arrows
    ShowNAMArrows();
  } else {
    // only one NAM in current folder, hide arrows
    HideNAMArrows();
  }
}

WDL_String FolderBrowser::StartNAMNavUp() {
  if ((l_iCurrentNAMIndex - 1) < 0) {
    // can't traverse further, return current
    return WDL_String(_oNAMs[l_iCurrentNAMIndex].c_str());
  } else {
    // load NAM that is -1 in list
    return WDL_String(_oNAMs[(l_iCurrentNAMIndex - 1)].c_str());
  }
}

int FolderBrowser::FinishNAMNavUp(std::string l_sMsg) {
  // TODO error messages like the IR loader.
  if (l_sMsg.size()) {
    std::stringstream l_oSS;
    l_oSS << "Failed to load NAM model. Message:\n\n"
          << l_sMsg << "\n\n"
          << "If the model is an old \"directory-style\" model, it "
             "can be "
             "converted using the utility at "
             "https://github.com/sdatkinson/nam-model-utility";
    lGraphics->ShowMessageBox(l_oSS.str().c_str(), "Failed to load model!",
                              kMB_OK);
  } else {
    if ((l_iCurrentNAMIndex - 1) < 0) {
      // can't traverse further, set to min array size
      l_iCurrentNAMIndex = 0;
    } else {
      // NAM was loaded successfully, set current index
      l_iCurrentNAMIndex--;
    }

    // set current NAM
    _oCurrentNAM = _oNAMs[l_iCurrentNAMIndex];

    // prevent user from clicking too fast
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return l_iCurrentNAMIndex;
}

WDL_String FolderBrowser::StartNAMNavDown() {
  if ((l_iCurrentNAMIndex + 1) > (_oNAMs.size() - 1)) {
    // can't traverse further, return current
    return WDL_String(_oNAMs[l_iCurrentNAMIndex].c_str());
  } else {
    // load NAM that is +1 in list
    return WDL_String(_oNAMs[(l_iCurrentNAMIndex + 1)].c_str());
  }
}

int FolderBrowser::FinishNAMNavDown(std::string l_sMsg) {
  // TODO error messages like the IR loader.
  if (l_sMsg.size()) {
    std::stringstream l_oSS;
    l_oSS << "Failed to load NAM model. Message:\n\n"
          << l_sMsg << "\n\n"
          << "If the model is an old \"directory-style\" model, it "
             "can be "
             "converted using the utility at "
             "https://github.com/sdatkinson/nam-model-utility";
    lGraphics->ShowMessageBox(l_oSS.str().c_str(), "Failed to load model!",
                              kMB_OK);
  } else {
    if ((l_iCurrentNAMIndex + 1) > (_oNAMs.size() - 1)) {
      // can't traverse further, set to max array size
      l_iCurrentNAMIndex = (_oNAMs.size() - 1);
    } else {
      // NAM was loaded successfully, set current index
      l_iCurrentNAMIndex++;
    }

    // set current NAM
    _oCurrentNAM = _oNAMs[l_iCurrentNAMIndex];

    // prevent user from clicking too fast
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return l_iCurrentNAMIndex;
}

void FolderBrowser::ShowNAMArrows() {
  IControl *l_oUpArrow =
      lGraphics->GetControlWithTag(250); // 250 = NAM up arrow
  l_oUpArrow->Hide(false);
  IControl *l_oDownArrow =
      lGraphics->GetControlWithTag(251); // 251 = NAM down arrow
  l_oDownArrow->Hide(false);
}

void FolderBrowser::HideNAMArrows() {
  IControl *l_oUpArrow =
      lGraphics->GetControlWithTag(250); // 250 = NAM up arrow
  l_oUpArrow->Hide(true);
  IControl *l_oDownArrow =
      lGraphics->GetControlWithTag(251); // 251 = NAM down arrow
  l_oDownArrow->Hide(true);
}

void FolderBrowser::InitializeIRNav(WDL_String fileName) {
  _oCurrentIR = fileName.Get();
  _oIRs.clear();

  // get all .wavs in folder of currently selected .wav
  const std::filesystem::path l_sPath =
      std::filesystem::current_path().u8string();
  std::set<std::filesystem::path> sorted_by_name; // sort listing

  for (const auto &p : std::filesystem::directory_iterator(l_sPath)) {
    if (!std::filesystem::is_directory(p)) {
      if (std::filesystem::path(p.path()).extension() == ".wav") {
        _oIRs.push_back(p.path().u8string());
      }
    }
  }

  // set current index for selected IR
  for (int i = 0; i < _oIRs.size(); i++) {
    if (_oIRs[i] == _oCurrentIR) {
      l_iCurrentIRIndex = i;
      break;
    }
  }

  if (_oIRs.size() > 1) {
    // more than one .wav in current folder, show arrows
    ShowIRArrows();
  } else {
    // only one .wav in current folder, hide arrows
    HideIRArrows();
  }
}

WDL_String FolderBrowser::StartIRNavUp() {
  if ((l_iCurrentIRIndex - 1) < 0) {
    // can't traverse further, return current
    return WDL_String(_oIRs[l_iCurrentIRIndex].c_str());
  } else {
    // load IR that is -1 in list
    return WDL_String(_oIRs[(l_iCurrentIRIndex - 1)].c_str());
  }
}

int FolderBrowser::FinishIRNavUp(dsp::wav::LoadReturnCode l_sMsg) {
  if (l_sMsg != dsp::wav::LoadReturnCode::SUCCESS) {
    std::stringstream message;
    message << "Failed to load IR file "
            << _oIRs[(l_iCurrentIRIndex - 1)].c_str() << ":\n";
    switch (l_sMsg) {
    case (dsp::wav::LoadReturnCode::ERROR_OPENING):
      message << "Failed to open file (is it being used by another "
                 "program?)";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_NOT_RIFF):
      message << "File is not a WAV file.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_NOT_WAVE):
      message << "File is not a WAV file.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_MISSING_FMT):
      message << "File is missing expected format chunk.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_INVALID_FILE):
      message << "WAV file contents are invalid.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_ALAW):
      message << "Unsupported file format \"A-law\"";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_MULAW):
      message << "Unsupported file format \"mu-law\"";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_EXTENSIBLE):
      message << "Unsupported file format \"extensible\"";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_NOT_MONO):
      message << "File is not mono.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_BITS_PER_SAMPLE):
      message << "Unsupported bits per sample";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_OTHER):
      message << "???";
      break;
    default:
      message << "???";
      break;
    }
    lGraphics->ShowMessageBox(message.str().c_str(), "Failed to load IR!",
                              kMB_OK);
  } else {
    if ((l_iCurrentIRIndex - 1) < 0) {
      // can't traverse further, set to min array size
      l_iCurrentIRIndex = 0;
    } else {
      // NAM was loaded successfully, set current index
      l_iCurrentIRIndex--;
    }

    // set current IR
    _oCurrentIR = _oIRs[l_iCurrentIRIndex];

    // prevent user from clicking too fast
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return l_iCurrentIRIndex;
}

WDL_String FolderBrowser::StartIRNavDown() {
  if ((l_iCurrentIRIndex + 1) > (_oIRs.size() - 1)) {
    // can't traverse further, return current
    return WDL_String(_oIRs[l_iCurrentIRIndex].c_str());
  } else {
    // load NAM that is +1 in list
    return WDL_String(_oIRs[(l_iCurrentIRIndex + 1)].c_str());
  }
}

int FolderBrowser::FinishIRNavDown(dsp::wav::LoadReturnCode l_sMsg) {
  if (l_sMsg != dsp::wav::LoadReturnCode::SUCCESS) {
    std::stringstream message;
    message << "Failed to load IR file "
            << _oIRs[(l_iCurrentIRIndex + 1)].c_str() << ":\n";
    switch (l_sMsg) {
    case (dsp::wav::LoadReturnCode::ERROR_OPENING):
      message << "Failed to open file (is it being used by another "
                 "program?)";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_NOT_RIFF):
      message << "File is not a WAV file.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_NOT_WAVE):
      message << "File is not a WAV file.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_MISSING_FMT):
      message << "File is missing expected format chunk.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_INVALID_FILE):
      message << "WAV file contents are invalid.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_ALAW):
      message << "Unsupported file format \"A-law\"";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_MULAW):
      message << "Unsupported file format \"mu-law\"";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_EXTENSIBLE):
      message << "Unsupported file format \"extensible\"";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_NOT_MONO):
      message << "File is not mono.";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_BITS_PER_SAMPLE):
      message << "Unsupported bits per sample";
      break;
    case (dsp::wav::LoadReturnCode::ERROR_OTHER):
      message << "???";
      break;
    default:
      message << "???";
      break;
    }
    lGraphics->ShowMessageBox(message.str().c_str(), "Failed to load IR!",
                              kMB_OK);
  } else {
    if ((l_iCurrentIRIndex + 1) > (_oIRs.size() - 1)) {
      // can't traverse further, set to max array size
      l_iCurrentIRIndex = (_oIRs.size() - 1);
    } else {
      // NAM was loaded successfully, set current index
      l_iCurrentIRIndex++;
    }

    // set current IR
    _oCurrentIR = _oIRs[l_iCurrentIRIndex];

    // prevent user from clicking too fast
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return l_iCurrentIRIndex;
}

void FolderBrowser::ShowIRArrows() {
  IControl *l_oUpArrow = lGraphics->GetControlWithTag(252); // 252 = IR up arrow
  l_oUpArrow->Hide(false);
  IControl *l_oDownArrow =
      lGraphics->GetControlWithTag(253); // 253 = IR down arrow
  l_oDownArrow->Hide(false);
}

void FolderBrowser::HideIRArrows() {
  IControl *l_oUpArrow = lGraphics->GetControlWithTag(252); // 252 = IR up arrow
  l_oUpArrow->Hide(true);
  IControl *l_oDownArrow =
      lGraphics->GetControlWithTag(253); // 253 = IR down arrow
  l_oDownArrow->Hide(true);
}
