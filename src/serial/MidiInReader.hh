#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"
#include "EventListener.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "openmsx.hh"
#include "circular_buffer.hh"
#include "Poller.hh"
#include <cstdio>
#include <mutex>
#include <thread>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;

class MidiInReader final : public MidiInDevice, private EventListener
{
public:
	MidiInReader(EventDistributor& eventDistributor, Scheduler& scheduler,
	             CommandController& commandController);
	~MidiInReader();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	const std::string& getName() const override;
	string_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	FileOperations::FILE_t file;
	cb_queue<byte> queue;
	std::mutex mutex; // to protect queue
	Poller poller;

	FilenameSetting readFilenameSetting;
};

} // namespace openmsx

#endif
