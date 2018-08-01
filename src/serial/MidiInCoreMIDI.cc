#if defined(__APPLE__)

#include "MidiInCoreMIDI.hh"
#include "MidiInConnector.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "serialize.hh"
#include "memory.hh"
#include "StringOp.hh"
#include <mach/mach_time.h>


namespace openmsx {

// MidiInCoreMIDI ===========================================================

void MidiInCoreMIDI::registerAll(EventDistributor& eventDistributor,
                                 Scheduler& scheduler,
                                 PluggingController& controller)
{
	ItemCount numberOfEndpoints = MIDIGetNumberOfSources();
	for (ItemCount i = 0; i < numberOfEndpoints; i++) {
		MIDIEndpointRef endpoint = MIDIGetSource(i);
		if (endpoint) {
			controller.registerPluggable(make_unique<MidiInCoreMIDI>(
					eventDistributor, scheduler, endpoint));
		}
	}
}

MidiInCoreMIDI::MidiInCoreMIDI(EventDistributor& eventDistributor_,
                               Scheduler& scheduler_, MIDIEndpointRef endpoint_)
	: eventDistributor(eventDistributor_)
	, scheduler(scheduler_)
	, endpoint(endpoint_)
{
	// Get a user-presentable name for the endpoint.
	CFStringRef midiDeviceName;
	OSStatus status = MIDIObjectGetStringProperty(
						endpoint, kMIDIPropertyDisplayName, &midiDeviceName);
	if (status) {
		status = MIDIObjectGetStringProperty(
						endpoint, kMIDIPropertyName, &midiDeviceName);
	}
	if (status) {
		name = "Nameless endpoint";
	} else {
		name = strCat(StringOp::fromCFString(midiDeviceName), " IN");
		CFRelease(midiDeviceName);
	}

	eventDistributor.registerEventListener(
			OPENMSX_MIDI_IN_COREMIDI_EVENT, *this);
}

MidiInCoreMIDI::~MidiInCoreMIDI()
{
	eventDistributor.unregisterEventListener(
			OPENMSX_MIDI_IN_COREMIDI_EVENT, *this);
}

void MidiInCoreMIDI::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(
			CFSTR("openMSX"), nullptr, nullptr, &client)) {
		throw PlugException("Failed to create MIDI client (", status, ')');
	}
	// Create input port.
	if (OSStatus status = MIDIInputPortCreate(
			client, CFSTR("Input"), sendPacketList, this, &port)) {
		MIDIClientDispose(client);
		client = 0;
		throw PlugException("Failed to create MIDI port (", status, ')');
	}

	MIDIPortConnectSource(port, endpoint, nullptr);
}

void MidiInCoreMIDI::unplugHelper(EmuTime::param /*time*/)
{
	// Dispose of the client; this automatically disposes of the port as well.
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", (int)status);
	}
	port = 0;
	client = 0;
}

const std::string& MidiInCoreMIDI::getName() const
{
	return name;
}

string_view MidiInCoreMIDI::getDescription() const
{
	return "Receives MIDI events from an existing CoreMIDI source.";
}

void MidiInCoreMIDI::sendPacketList(const MIDIPacketList *packetList,
                                    void *readProcRefCon, void *srcConnRefCon)
{
	((MidiInCoreMIDI*)readProcRefCon)
			->sendPacketList(packetList, srcConnRefCon);
}

void MidiInCoreMIDI::sendPacketList(const MIDIPacketList *packetList,
                                    void * /*srcConnRefCon*/) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		const MIDIPacket *packet = &packetList->packet[0];
		for (UInt32 i = 0; i < packetList->numPackets; i++) {
			for (UInt16 j = 0; j < packet->length; j++) {
				queue.push_back(packet->data[j]);
			}
			packet = MIDIPacketNext(packet);
		}
	}
	eventDistributor.distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_MIDI_IN_COREMIDI_EVENT));
}

// MidiInDevice
void MidiInCoreMIDI::signal(EmuTime::param time)
{
	auto connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		return;
	}
	if (!connector->ready()) {
		return;
	}

	byte data;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	connector->recvByte(data, time);
}

// EventListener
int MidiInCoreMIDI::signalEvent(const std::shared_ptr<const Event>& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
	}
	return 0;
}

template<typename Archive>
void MidiInCoreMIDI::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiInCoreMIDI);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInCoreMIDI, "MidiInCoreMIDI");


// MidiInCoreMIDIVirtual ====================================================

MidiInCoreMIDIVirtual::MidiInCoreMIDIVirtual(EventDistributor& eventDistributor_,
                                             Scheduler& scheduler_)
	: eventDistributor(eventDistributor_)
	, scheduler(scheduler_)
	, client(0)
	, endpoint(0)
{
	eventDistributor.registerEventListener(
			OPENMSX_MIDI_IN_COREMIDI_VIRTUAL_EVENT, *this);
}

MidiInCoreMIDIVirtual::~MidiInCoreMIDIVirtual()
{
	eventDistributor.unregisterEventListener(
			OPENMSX_MIDI_IN_COREMIDI_VIRTUAL_EVENT, *this);
}

void MidiInCoreMIDIVirtual::plugHelper(Connector& /*connector*/,
                                       EmuTime::param /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"),
	                                       nullptr, nullptr, &client)) {
		throw PlugException("Failed to create MIDI client (", status, ')');
	}
	// Create endpoint.
	if (OSStatus status = MIDIDestinationCreate(client, CFSTR("openMSX"),
	                                            sendPacketList, this,
	                                            &endpoint)) {
		MIDIClientDispose(client);
		throw PlugException("Failed to create MIDI endpoint (", status, ')');
	}
}

void MidiInCoreMIDIVirtual::unplugHelper(EmuTime::param /*time*/)
{
	if (OSStatus status = MIDIEndpointDispose(endpoint)) {
		fprintf(stderr, "Failed to dispose of MIDI port (%d)\n", (int)status);
	}
	endpoint = 0;
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", (int)status);
	}
	client = 0;
}

const std::string& MidiInCoreMIDIVirtual::getName() const
{
	static const std::string name("Virtual IN");
	return name;
}

string_view MidiInCoreMIDIVirtual::getDescription() const
{
	return "Sends MIDI events from a newly created CoreMIDI virtual source.";
}

void MidiInCoreMIDIVirtual::sendPacketList(const MIDIPacketList *packetList,
                                           void *readProcRefCon,
                                           void *srcConnRefCon)
{
	((MidiInCoreMIDIVirtual*)readProcRefCon)
			->sendPacketList(packetList, srcConnRefCon);
}

void MidiInCoreMIDIVirtual::sendPacketList(const MIDIPacketList *packetList,
                                           void * /*srcConnRefCon*/)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		const MIDIPacket *packet = &packetList->packet[0];
		for (UInt32 i = 0; i < packetList->numPackets; i++) {
			for (UInt16 j = 0; j < packet->length; j++) {
				queue.push_back(packet->data[j]);
			}
			packet = MIDIPacketNext(packet);
		}
	}
	eventDistributor.distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_MIDI_IN_COREMIDI_VIRTUAL_EVENT));
}

// MidiInDevice
void MidiInCoreMIDIVirtual::signal(EmuTime::param time)
{
	auto connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		return;
	}
	if (!connector->ready()) {
		return;
	}

	byte data;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	connector->recvByte(data, time);
}

// EventListener
int MidiInCoreMIDIVirtual::signalEvent(
		const std::shared_ptr<const Event>& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
	}
	return 0;
}

template<typename Archive>
void MidiInCoreMIDIVirtual::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiInCoreMIDIVirtual);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInCoreMIDIVirtual, "MidiInCoreMIDIVirtual");

} // namespace openmsx

#endif // defined(__APPLE__)
