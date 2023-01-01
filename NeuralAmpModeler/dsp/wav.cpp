//
//  wav.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/31/22.
//

#include <cmath>  // pow
#include <iostream>
#include <fstream>
#include <vector>

#include "wav.h"

int dsp::wav::Load(const WDL_String &fileName, std::vector<float> &audio, double &sampleRate)
{
  // Open the WAV file for reading
  std::ifstream wavFile(fileName.Get(), std::ios::binary);
  
  // Check if the file was opened successfully
  if (!wavFile.is_open()) {
    std::cerr << "Error opening WAV file" << std::endl;
    return dsp::wav::RET_ERROR_OPENING;
  }
  
  // WAV file has 3 "chunks": RIFF ("RIFF"), format ("fmt ") and data ("data").
  // Read the WAV file header
  char chunkId[4];
  wavFile.read(chunkId, 4);
  if (strncmp(chunkId, "RIFF", 4) != 0) {
    std::cerr << "Error: Not a WAV file" << std::endl;
    return dsp::wav::RET_ERROR_NOT_WAV;
  }
  
  int chunkSize;
  wavFile.read(reinterpret_cast<char*>(&chunkSize), 4);
  
  char format[4];
  wavFile.read(format, 4);
  if (strncmp(format, "WAVE", 4) != 0) {
    std::cerr << "Error: Not a WAV file" << std::endl;
    return dsp::wav::RET_ERROR_NOT_WAV;
  }
  
  // Read the format chunk
  char subchunk1Id[4];
  wavFile.read(subchunk1Id, 4);
  if (strncmp(subchunk1Id, "fmt ", 4) != 0) {
    std::cerr << "Error: Invalid WAV file" << std::endl;
    return dsp::wav::RET_ERROR_INVALID_WAV;
  }
  
  int subchunk1Size;
  wavFile.read(reinterpret_cast<char*>(&subchunk1Size), 4);
  
  short audioFormat;
  wavFile.read(reinterpret_cast<char*>(&audioFormat), 2);
  if (audioFormat != 1) {
    std::cerr << "Error: Only PCM format is supported" << std::endl;
    return dsp::wav::RET_ERROR_NOT_PCM;
  }
  
  short numChannels;
  wavFile.read(reinterpret_cast<char*>(&numChannels), 2);
  // HACK
  if (numChannels != 1) {
    std::cerr << "Require mono (using for IR loading)" << std::endl;
    return dsp::wav::RET_ERROR_INVALID_WAV;
  }
  
  int iSampleRate;
  wavFile.read(reinterpret_cast<char*>(&iSampleRate), 4);
  // Store in format we assume (SR is double)
  sampleRate = (double) iSampleRate;
  
  int byteRate;
  wavFile.read(reinterpret_cast<char*>(&byteRate), 4);
  
  short blockAlign;
  wavFile.read(reinterpret_cast<char*>(&blockAlign), 2);
  
  short bitsPerSample;
  wavFile.read(reinterpret_cast<char*>(&bitsPerSample), 2);
  
  // Read the data chunk
  char subchunk2Id[4];
  wavFile.read(subchunk2Id, 4);
  if (strncmp(subchunk2Id, "data", 4) != 0) {
    std::cerr << "Error: Invalid WAV file" << std::endl;
    return dsp::wav::RET_ERROR_INVALID_WAV;
  }
  
  // Size of the data chunk, in bits.
  int subchunk2Size;
  wavFile.read(reinterpret_cast<char*>(&subchunk2Size), 4);
  
  if (bitsPerSample == 16)
    dsp::wav::_LoadSamples16(wavFile, subchunk2Size, audio);
  else if (bitsPerSample == 24)
    dsp::wav::_LoadSamples24(wavFile, subchunk2Size, audio);
  else if (bitsPerSample == 32)
    dsp::wav::_LoadSamples32(wavFile, subchunk2Size, audio);
  else {
    std::cerr << "Error: Unsupported bits per sample: " << bitsPerSample << std::endl;
    return 1;
  }
  
  // Close the WAV file
  wavFile.close();
  
  // Print the number of samples
  // std::cout << "Number of samples: " << samples.size() << std::endl;
  
  return dsp::wav::RET_SUCCESS;
}

void dsp::wav::_LoadSamples16(std::ifstream &wavFile,
                              const int chunkSize,
                              std::vector<float>& samples)
{
  // Allocate an array to hold the samples
  std::vector<short> tmp(chunkSize / 2);  // 16 bits (2 bytes) per sample
  
  // Read the samples from the file into the array
  wavFile.read(reinterpret_cast<char*>(tmp.data()), chunkSize);
  
  // Copy into the return array
  const float scale = 1.0 / ((double) (1 << 15));
  samples.resize(tmp.size());
  for (auto i=0; i<samples.size(); i++)
    samples[i] = scale * ((float) tmp[i]);  // 2^16
}

void dsp::wav::_LoadSamples24(std::ifstream &wavFile,
                               const int chunkSize,
                               std::vector<float>& samples)
{
  // Allocate an array to hold the samples
  std::vector<int> tmp(chunkSize / 3);  // 24 bits (3 bytes) per sample
  // Read in and convert the samples
  for (int& x : tmp) {
    x = dsp::wav::_ReadSigned24BitInt(wavFile);
  }
  
  // Copy into the return array
  const float scale = 1.0 / ((double) (1 << 23));
  samples.resize(tmp.size());
  for (auto i=0; i<samples.size(); i++)
    samples[i] = scale * ((float) tmp[i]);
}

int dsp::wav::_ReadSigned24BitInt(std::ifstream& stream) {
  // Read the three bytes of the 24-bit integer.
  std::uint8_t bytes[3];
  stream.read(reinterpret_cast<char*>(bytes), 3);
  
  // Combine the three bytes into a single integer using bit shifting and masking.
  // This works by isolating each byte using a bit mask (0xff) and then shifting
  // the byte to the correct position in the final integer.
  int value = bytes[0] | (bytes[1] << 8) | (bytes[2]<<16);
  
  // The value is stored in two's complement format, so if the most significant
  // bit (the 24th bit) is set, then the value is negative. In this case, we
  // need to extend the sign bit to get the correct negative value.
  if (value & (1 << 23)) {
    value |= ~((1 << 24) - 1);
  }
  
  return value;
}


void dsp::wav::_LoadSamples32(std::ifstream &wavFile,
                              const int chunkSize,
                              std::vector<float>& samples)
{
  // NOTE: 32-bit is float.
  samples.resize(chunkSize / 4);  // 32 bits (4 bytes) per sample
  // Read the samples from the file into the array
  wavFile.read(reinterpret_cast<char*>(samples.data()), chunkSize);
}
