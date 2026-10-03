#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  VELUX WLR 100 IR protocol  ->  WLI 130 51 keypad / receiver
// ---------------------------------------------------------------------------
//  Carrier: 32132 Hz (31.12 us period). Each bit takes exactly 52 carrier
//  cycles (~1618 us); only how many of those cycles are modulated changes:
//
//      bit 1 -> 40 burst cycles (1245 us) + 12 idle (373 us)
//      bit 0 -> 14 burst cycles  (436 us) + 38 idle (1183 us)
//
//  Frame: 24 bits, first bit transmitted = MSB.
//
//    bit  1..3   ACTION
//                  001 open,  full travel      (AUTO)
//                  010 open,  while held       (MANUAL)
//                  011 close, full travel      (AUTO)
//                  100 close, while held       (MANUAL)
//                  101 stop
//    bit  4..6   MOTOR, one-hot:
//                  001 motor 1 (window)
//                  010 motor 2 (blind)
//                  100 motor 3 (awning)
//                  111 all motors
//    bit  7..10  motor set 1..10 in binary (0000 = all sets)
//    bit 11..20  security code = the WLI 130 / WLR 100's 10 DIP switches
//    bit 21..24  checksum
//
//  The checksum: public sources considered it unsolved. Comparing two sets of
//  real frames captured with DIFFERENT security codes (1000000000 and
//  1011100000) shows the 4 bits do not change: the checksum does NOT depend
//  on the security code. It is an affine (XOR) function of bits 1..10 only,
//  verified against 21 real frames:
//
//    chk = base[motor] ^ (bit1 ? 1010 : 0) ^ (bit2 ? 0101 : 0) ^ (bit3 ? 1011 : 0)
//
//  with base[001]=0011, base[010]=0110, base[100]=1100, base[111]=1001.
//  Valid for set = 0000, the only value present in every known sample and
//  also the one that addresses all sets - which is what's needed 99% of the
//  time. This lets us GENERATE commands for any security code, without
//  needing the original WLR 100 remote.
// ---------------------------------------------------------------------------

namespace velux {

constexpr uint16_t kCarrierHz   = 32132;
constexpr uint16_t kMarkOneUs   = 1245;   // 40 cycles
constexpr uint16_t kSpaceOneUs  = 373;    // 12 cycles
constexpr uint16_t kMarkZeroUs  = 436;    // 14 cycles
constexpr uint16_t kSpaceZeroUs = 1183;   // 38 cycles
constexpr uint8_t  kBits        = 24;
constexpr uint16_t kRawLen      = kBits * 2;
constexpr uint16_t kRepeatGapMs = 19;     // gap between the two copies of a frame

// Field positions inside a uint32_t (frame bit 1 = bit 23).
constexpr uint8_t kShAct = 21;  constexpr uint32_t kMskAct = 0x7u   << kShAct;
constexpr uint8_t kShMot = 18;  constexpr uint32_t kMskMot = 0x7u   << kShMot;
constexpr uint8_t kShSet = 14;  constexpr uint32_t kMskSet = 0xFu   << kShSet;
constexpr uint8_t kShSec = 4;   constexpr uint32_t kMskSec = 0x3FFu << kShSec;
constexpr uint8_t kShChk = 0;   constexpr uint32_t kMskChk = 0xFu   << kShChk;

enum Action : uint8_t {
  ACT_OPEN_AUTO    = 0b001,
  ACT_OPEN_MANUAL  = 0b010,
  ACT_CLOSE_AUTO   = 0b011,
  ACT_CLOSE_MANUAL = 0b100,
  ACT_STOP         = 0b101,
};

constexpr uint8_t kMotorWindow = 0b001;   // motor 1
constexpr uint8_t kMotorBlind  = 0b010;   // motor 2
constexpr uint8_t kMotorAwning = 0b100;   // motor 3
constexpr uint8_t kMotorAll    = 0b111;

struct Frame {
  uint8_t  action;     // 3 bits, see enum Action
  uint8_t  motor;      // 3 bits, one-hot
  uint8_t  set;        // 0..10 (0 = all sets)
  uint16_t security;   // 10 bits
  uint8_t  chk;        // 4 bits
};

// Checksum of the final 4 bits. Valid for set == 0.
// Note: base[0b111] (all motors) is the only value obtained by linear
// extrapolation and never observed on a real frame; the other three are
// verified against 21 real frames.
inline uint8_t checksum(uint8_t action, uint8_t motor) {
  //                             -     m1      m2     -      m3    -  -   all
  static const uint8_t base[8] = {0, 0b0011, 0b0110, 0, 0b1100, 0, 0, 0b1001};
  uint8_t v = base[motor & 0x7];
  if (action & 0b100) v ^= 0b1010;   // bit 1
  if (action & 0b010) v ^= 0b0101;   // bit 2
  if (action & 0b001) v ^= 0b1011;   // bit 3
  return v & 0xF;
}

// True if the checksum for this motor/set combination can be computed.
inline bool checksumKnown(uint8_t motor, uint8_t set) {
  if (set != 0) return false;
  return motor == kMotorWindow || motor == kMotorBlind ||
         motor == kMotorAwning || motor == kMotorAll;
}

inline Frame decode(uint32_t f) {
  Frame r;
  r.action   = (f & kMskAct) >> kShAct;
  r.motor    = (f & kMskMot) >> kShMot;
  r.set      = (f & kMskSet) >> kShSet;
  r.security = (f & kMskSec) >> kShSec;
  r.chk      = (f & kMskChk) >> kShChk;
  return r;
}

inline uint32_t encode(const Frame& r) {
  uint32_t f = 0;
  f |= (uint32_t)(r.action   & 0x7)   << kShAct;
  f |= (uint32_t)(r.motor    & 0x7)   << kShMot;
  f |= (uint32_t)(r.set      & 0xF)   << kShSet;
  f |= (uint32_t)(r.security & 0x3FF) << kShSec;
  f |= (uint32_t)(r.chk      & 0xF)   << kShChk;
  return f;
}

// Builds a complete command, checksum included. set = 0 -> all sets.
inline uint32_t build(uint8_t action, uint8_t motor, uint16_t security,
                      uint8_t set = 0) {
  Frame r;
  r.action   = action;
  r.motor    = motor;
  r.set      = set;
  r.security = security;
  r.chk      = checksum(action, motor);
  return encode(r);
}

// Does a frame's checksum (e.g. one typed into the web UI's raw-frame box)
// match what we compute ourselves?
inline bool checksumOk(uint32_t f) {
  const Frame r = decode(f);
  if (!checksumKnown(r.motor, r.set)) return true;   // not verifiable
  return r.chk == checksum(r.action, r.motor);
}

// Builds the mark/space sequence (us), one pair per bit, MSB first.
inline uint16_t toRaw(uint32_t frame, uint16_t* out) {
  uint16_t n = 0;
  for (int8_t i = kBits - 1; i >= 0; --i) {
    const bool bit = (frame >> i) & 1;
    out[n++] = bit ? kMarkOneUs  : kMarkZeroUs;
    out[n++] = bit ? kSpaceOneUs : kSpaceZeroUs;
  }
  return n;   // = kRawLen
}

inline const char* motorName(uint8_t m) {
  switch (m) {
    case kMotorWindow: return "window (motor 1)";
    case kMotorBlind:  return "blind (motor 2)";
    case kMotorAwning: return "awning (motor 3)";
    case kMotorAll:    return "all motors";
    default:           return "motor ?";
  }
}

inline const char* actionName(uint8_t a) {
  switch (a) {
    case ACT_OPEN_AUTO:    return "OPEN (full)";
    case ACT_OPEN_MANUAL:  return "OPEN (manual)";
    case ACT_CLOSE_AUTO:   return "CLOSE (full)";
    case ACT_CLOSE_MANUAL: return "CLOSE (manual)";
    case ACT_STOP:         return "STOP";
    default:               return "action ?";
  }
}

}  // namespace velux
