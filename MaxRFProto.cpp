#include "MaxRFProto.h"
#include "Arduino.h"

/* TStreaming Formatting type to use for the field titles in print
 * output. */
typedef Align<16> Title;

/**
 * Static list of known devices.
 */
Device devices[] = {
  /* Add your devices here, for example: */
  //{0x00b825, DeviceType::CUBE, "cube", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0},
  //{0x0298e5, DeviceType::WALL, "wall", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0},
  //{0x04c8dd, DeviceType::RADIATOR, "up  ", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0, {.radiator = {Mode::UNKNOWN, VALVE_UNKNOWN}}},
  //{0x0131b4, DeviceType::RADIATOR, "down", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0, {.radiator = {Mode::UNKNOWN, VALVE_UNKNOWN}}},
};

/* Find or assign a device struct based on the address */
static Device *get_device(uint32_t addr, DeviceType type) {
  for (int i = 0; i < lengthof(devices); ++i) {
    /* The address is not in the list yet, assign this empty slot. */
    if (devices[i].address == 0 && addr != 0) {
      devices[i].address = addr;
      devices[i].type = type;
      devices[i].name = NULL;
    }
    /* Found it */
    if (devices[i].address == addr)
      return &devices[i];
  }
  /* Not found and no slots left */
  return NULL;
}

/* MaxRFMessage */
const FlashString *MaxRFMessage::mode_to_str(Mode mode) {
  switch (mode) {
    case Mode::AUTO: return F("auto");
    case Mode::MANUAL: return F("manual");
    case Mode::TEMPORARY: return F("temporary");
    case Mode::BOOST: return F("boost");
    default: return F("");
  }
};

const FlashString *MaxRFMessage::display_mode_to_str(DisplayMode display_mode) {
  switch(display_mode) {
    case DisplayMode::SET_TEMP: return F("Set temperature");
    case DisplayMode::ACTUAL_TEMP: return F("Actual temperature");
    default: return F("");
  }
};

const FlashString *MaxRFMessage::type_to_str(MessageType type) {
  switch(type) {
    case MessageType::PAIR_PING:                      return F("PairPing");
    case MessageType::PAIR_PONG:                      return F("PairPong");
    case MessageType::ACK:                            return F("Ack");
    case MessageType::TIME_INFORMATION:               return F("TimeInformation");
    case MessageType::CONFIG_WEEK_PROFILE:            return F("ConfigWeekProfile");
    case MessageType::CONFIG_TEMPERATURES:            return F("ConfigTemperatures");
    case MessageType::CONFIG_VALVE:                   return F("ConfigValve");
    case MessageType::ADD_LINK_PARTNER:               return F("AddLinkPartner");
    case MessageType::REMOVE_LINK_PARTNER:            return F("RemoveLinkPartner");
    case MessageType::SET_GROUP_ID:                   return F("SetGroupId");
    case MessageType::REMOVE_GROUP_ID:                return F("RemoveGroupId");
    case MessageType::SHUTTER_CONTACT_STATE:          return F("ShutterContactState");
    case MessageType::SET_TEMPERATURE:                return F("SetTemperature");
    case MessageType::WALL_THERMOSTAT_STATE:          return F("WallThermostatState");
    case MessageType::SET_COMFORT_TEMPERATURE:        return F("SetComfortTemperature");
    case MessageType::SET_ECO_TEMPERATURE:            return F("SetEcoTemperature");
    case MessageType::PUSH_BUTTON_STATE:              return F("PushButtonState");
    case MessageType::THERMOSTAT_STATE:               return F("ThermostatState");
    case MessageType::SET_DISPLAY_ACTUAL_TEMPERATURE: return F("SetDisplayActualTemperature");
    case MessageType::WAKE_UP:                        return F("WakeUp");
    case MessageType::RESET:                          return F("Reset");
    default:                             return F("Unknown");
  }
}

DeviceType MaxRFMessage::message_type_to_sender_type(MessageType type) {
  switch(type) {
    case MessageType::WALL_THERMOSTAT_STATE:          return DeviceType::WALL;
    case MessageType::THERMOSTAT_STATE:               return DeviceType::RADIATOR;
    case MessageType::SET_DISPLAY_ACTUAL_TEMPERATURE: return DeviceType::CUBE;
    default:                             return DeviceType::UNKNOWN;
  }
}

MaxRFMessage *MaxRFMessage::create_message_from_type(MessageType type) {
  switch(type) {
    case MessageType::SET_TEMPERATURE:                return new SetTemperatureMessage();
    case MessageType::WALL_THERMOSTAT_STATE:          return new WallThermostatStateMessage();
    case MessageType::THERMOSTAT_STATE:               return new ThermostatStateMessage();
    case MessageType::SET_DISPLAY_ACTUAL_TEMPERATURE: return new SetDisplayActualTemperatureMessage();
    case MessageType::ACK:                            return new AckMessage();
    default:                             return new UnknownMessage();
  }
}

MaxRFMessage *MaxRFMessage::parse(const uint8_t *buf, size_t len) {
  if (len < 10)
    return NULL;

  MessageType type = (MessageType)buf[2];
  MaxRFMessage *m = create_message_from_type(type);

  m->seqnum = buf[0];
  m->flags = buf[1];
  m->type = type;
  m->addr_from = getBits(buf + 3, 0, RF_ADDR_SIZE);
  m->addr_to = getBits(buf + 6, 0, RF_ADDR_SIZE);
  m->group_id = buf[9];

  m->from = get_device(m->addr_from, message_type_to_sender_type(type));
  m->to = get_device(m->addr_to, DeviceType::UNKNOWN);

  if (m->parse_payload(buf + 10, len - 10))
    return m;
  else
    return NULL;
}

size_t MaxRFMessage::printTo(Print &p) const {
  p << V<Title>(F("Sequence num:")) << V<Hex>(this->seqnum) << "\r\n";
  p << V<Title>(F("Flags:")) << V<Hex>(this->flags) << "\r\n";
  p << V<Title>(F("Packet type:")) << V<Hex>(this->type)
    << " (" << type_to_str(this->type) << ")" << "\r\n";
  p << V<Title>(F("Packet from:")) << V<Address>(this->addr_from);
  if (this->from)
    p << " (" << this->from->name << ")";
  p << "\r\n";
  p << V<Title>(F("Packet to:")) << V<Address>(this->addr_to);
  if (this->to)
    p << " (" << this->to->name << ")";
  p << "\r\n";
  p << V<Title>(F("Group id:")) << V<Hex>(this->group_id) << "\r\n";

  return 0; /* XXX */
}

void MaxRFMessage::updateState() {
  /* Nothing to do */
}

/* UnknownMessage */
bool UnknownMessage::parse_payload(const uint8_t *buf, size_t len) {
  this->payload = buf;
  this->payload_len = len;
  return true;
}

size_t UnknownMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Payload:"))
    << V<Array<Hex, TChar<' '>>>(this->payload, this->payload_len)
    << "\r\n";
}

/* SetTemperatureMessage */

bool SetTemperatureMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 1)
    return false;

  this->set_temp = buf[0] & 0x3f;
  this->mode = (Mode) ((buf[0] >> 6) & 0x3);

  if (len >= 4)
    this->until = new UntilTime(buf + 1);
  else
    this->until = NULL;

  return true;
}

size_t SetTemperatureMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Mode:")) << mode_to_str(this->mode) << "\r\n";
  p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";
  if (this->until) {
    p << V<Title>(F("Until:")) << *(this->until) << "\r\n";
  }

  return 0; /* XXX */
}

/* WallThermostatStateMessage */

bool WallThermostatStateMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 2)
    return false;

  this->set_temp = buf[0] & 0x7f;
  this->actual_temp = ((buf[0] & 0x80) << 1) |  buf[1];
  /* Note that mode and until time are not in this message */

  return true;
}

size_t WallThermostatStateMessage::printTo(Print &p) const {
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";
  p << V<Title>(F("Actual temp:")) << V<ActualTemp>(this->actual_temp) << "\r\n";

  return 0; /* XXX */
}

void WallThermostatStateMessage::updateState() {
  MaxRFMessage::updateState();
  this->from->set_temp = this->set_temp;
  this->from->actual_temp = this->actual_temp;
  this->from->actual_temp_time = millis();
}

/* ThermostatStateMessage */

bool ThermostatStateMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 3)
    return false;

  this->mode = (Mode) (buf[0] & 0x3);
  this->dst = (buf[0] >> 2) & 0x1;
  this->locked = (buf[0] >> 5) & 0x1;
  this->battery_low = (buf[0] >> 7) & 0x1;
  this->valve_pos = buf[1];
  this->set_temp = buf[2];

  this->actual_temp = 0;
  if (this->mode != Mode::TEMPORARY && len >= 5)
    this->actual_temp = ((buf[3] & 0x1) << 8) + buf[4];

  this->until = NULL;
  if (this->mode == Mode::TEMPORARY && len >= 6)
    this->until = new UntilTime(buf + 3);

  return true;
}

size_t ThermostatStateMessage::printTo(Print &p) const {
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Mode:")) << mode_to_str(this->mode) << "\r\n";
  p << V<Title>(F("Adjust to DST:")) << this->dst << "\r\n";
  p << V<Title>(F("Locked:")) << this->locked << "\r\n";
  p << V<Title>(F("Battery Low:")) << this->battery_low << "\r\n";
  p << V<Title>(F("Valve position:")) << this->valve_pos << "%" << "\r\n";
  p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";

  if (this->actual_temp)
    p << V<Title>(F("Actual temp:")) << V<ActualTemp>(this->actual_temp) << "\r\n";

  if (this->until)
    p << V<Title>(F("Until:")) << *(this->until) << "\r\n";

  return 0; /* XXX */
}

void ThermostatStateMessage::updateState() {
  this->from->set_temp = this->set_temp;
  this->from->data.radiator.valve_pos = this->valve_pos;
  if (this->actual_temp) {
    this->from->actual_temp = this->actual_temp;
    this->from->actual_temp_time = millis();
  }
}

/* SetDisplayActualTemperatureMessage */
bool SetDisplayActualTemperatureMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 1)
    return NULL;
  this->display_mode = (DisplayMode) ((buf[0] >> 2) & 0x1);
  return true;
}

size_t SetDisplayActualTemperatureMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Display mode:")) << display_mode_to_str(this->display_mode) << "\r\n";
}

/* AckMessage */
bool AckMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 4)
    return false;

  /* XXX: Perhaps buf[0] == 0x01 can be used here instead? */
  if (this->from && this->from->type == DeviceType::RADIATOR) {
    /* We only know about packet formats sent by radiators yet */

    this->mode = (Mode) (buf[1] & 0x3);
    this->dst = (buf[1] >> 2) & 0x1;
    /* The locked and battery_low bits are unconfirmed, but they probably
     * match the RadiatorThermostateStateMessage. */
    this->locked = (buf[1] >> 5) & 0x1;
    this->battery_low = (buf[1] >> 7) & 0x1;
    this->valve_pos = buf[2];
    this->set_temp = buf[3];

    this->until = NULL;
    if (this->mode == Mode::TEMPORARY && len >= 7)
      this->until = new UntilTime(buf + 4);
  }

  return true;
}

size_t AckMessage::printTo(Print &p) const {
  MaxRFMessage::printTo(p);
  if (this->from && this->from->type == DeviceType::RADIATOR) {
    p << V<Title>(F("Mode:")) << mode_to_str(this->mode) << "\r\n";
    p << V<Title>(F("Adjust to DST:")) << this->dst << "\r\n";
    p << V<Title>(F("Locked:")) << this->locked << "\r\n";
    p << V<Title>(F("Battery Low:")) << this->battery_low << "\r\n";
    p << V<Title>(F("Valve position:")) << this->valve_pos << "%" << "\r\n";
    p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";

    if (this->until)
      p << V<Title>(F("Until:")) << *(this->until) << "\r\n";
  }

  return 0; /* XXX */
}

void AckMessage::updateState() {
  if (this->from && this->from->type == DeviceType::RADIATOR) {
    this->from->set_temp = this->set_temp;
    this->from->data.radiator.valve_pos = this->valve_pos;
  }
}

/* UntilTime */

UntilTime::UntilTime(const uint8_t *buf) {
  this->year = buf[1] & 0x3f;
  this->month = ((buf[0] & 0xE0) >> 4) | (buf[1] >> 7);
  this->day = buf[0] & 0x1f;
  this->time = buf[2] & 0x3f;
}

size_t UntilTime::printTo(Print &p) const {
  p << "20" << V<Number<2>>(this->year) << "." << V<Number<2>>(this->month) << "." << V<Number<2>>(this->day);
  p << " " << V<Number<2>>(this->time / 2) << (this->time % 2 ? ":30" : ":00");
}


/*
Sequence num:   E4
Flags:          04
Packet type:    70 (Unknown)
Packet from:    0298E5
Packet to:      000000
Group id:       00
Payload:        19 04 2A 00 CD

19: DST switch, mode = auto
    0:1 mode
    2   DST switch
    5   ??
04: Display mode?
2A: set temp (21°)
00 CD: Actual temp (20.5°)

Sequence num:   9C
Flags:          04
Packet type:    70 (Unknown)
Packet from:    0298E5
Packet to:      000000
Group id:       00
Payload:        12 04 24 48 0D 1B

19: DST switch, mode = temporary
    0:1 mode
    2   DST switch
    5   ??
04: Display mode?
24: set temp (18°)
48 0D 1B: until time

Perhaps 70 is really WallThermostatState and the curren WallThermostatState is
more of a "update temp" message? It seems 70 is sent when the SetTemp of a WT
changes.

*/

/*
Set DST adjust
Sequence num:   E5
Flags:          00
Packet type:    81 (Unknown)
Packet from:    00B825
Packet to:      0298E5
Group id:       00
Payload:        00
00: Disable
01: Enable

Sent to radiator thermostats only?

*/

/*

Ack from radiator thermostat:

Sequence num:   2C
Flags:          02
Packet type:    02 (Ack)
Packet from:    0298E5
Packet to:      04C8DD
Group id:       00
Payload:        01 11 00 28
01: 1 == more data? 0 == no data??
11: flags, same as 11/19 in type 70?
00: Valve position / displaymode flags
28: Set temp

Sequence num:   1B
Flags:          02
Packet type:    02 (Ack)
Packet from:    0298E5
Packet to:      00B825
Group id:       00
Payload:        01 12 04 24 48 0D 1B

01: 1 == more data? 0 == no data??
11: flags, same as 11/19 in type 70? x2 == temporary
00: Valve position / displaymode flags
24: Set temp (18.0°)
48 0D 1B: Until time


Ack from wall thermostat to SetTemperature:

Sequence num:   4F
Flags:          00
Packet type:    02 (Ack)
Packet from:    00B825
Packet to:      0298E5
Group id:       00
Payload:        00

00: ???
*/

/* vim: set sw=2 sts=2 expandtab: */
