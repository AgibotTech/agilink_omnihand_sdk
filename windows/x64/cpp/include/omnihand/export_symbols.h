// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

/**
 * @file export_symbols.h
 * @brief Export symbol macro definitions
 * @author hanjun
 * @date 25-8-1
 **/

#ifndef AGILINK_EXPORT_SYMBOLS_H
#define AGILINK_EXPORT_SYMBOLS_H

#if defined(_WIN32) || defined(_WIN64)
  #ifdef BUILDING_DLL
    #define AGIBOT_EXPORT __declspec(dllexport)
  #else
    #define AGIBOT_EXPORT __declspec(dllimport)
  #endif
#else
  #define AGIBOT_EXPORT __attribute__((visibility("default")))
#endif

#endif  // AGILINK_EXPORT_SYMBOLS_H