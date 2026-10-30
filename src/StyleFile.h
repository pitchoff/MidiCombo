#pragma once

#include "Style.h"

#include <QString>

#include <vector>

// Reads band styles from an XML file. The pattern syntax is the one of
// mc::Style (see Style.h); spaces and '|' inside patterns are ignored.
//
// <midicombo-styles version="1">
//   <style name="Country Two-Step">
//     <meter beats="4" steps-per-beat="2" bars="2"/>
//     <tempo>160</tempo>
//     <swing amount="0" unit="8"/>          <!-- amount in percent -->
//     <crash drum="crash"/>                 <!-- drum="none" disables it -->
//     <drums>
//       <row drum="kick">X... X... | X... X...</row>
//       <row note="38">..X. ..X. | ..X. ..X.</row>
//     </drums>
//     <fill>                                <!-- optional, one bar -->
//       <row drum="snare">..x. xxXX</row>
//     </fill>
//     <bass program="32" velocity="96">R-.. 5-.. | R-.. 5-A-</bass>
//     <comp program="25" velocity="78">..C- ..C- | ..C- ..C-</comp>
//                                           <!-- voicing="power": root, fifth, octave -->
//     <pad program="22" enabled="false"/>
//   </style>
// </midicombo-styles>
//
// Drum rows take either a GM note number (note="38") or a drum name
// (drum="snare"); drumNames() lists the names.
bool readStyleFile(const QString &path, std::vector<mc::Style> &styles, QString &error);
bool writeStyleFile(const QString &path, const std::vector<mc::Style> &styles, QString &error);
QStringList drumNames();
// The name of a GM drum note ("snare"), or an empty string.
QString drumName(int note);
int drumNote(const QString &name); // -1 if unknown
