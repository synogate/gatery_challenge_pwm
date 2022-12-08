#include <gatery/frontend.h>
#include <gatery/utils.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/ReferenceSimulator.h>

#include <iostream>

using namespace gtry;
using namespace gtry::vhdl;
using namespace gtry::scl;
using namespace gtry::utils; 


/**
 * @brief Implement the challenge here!
 * @details In this coding challenge, the goal is to implement PWM in gatery.
 *	Given an UInt signal that encodes the duty cycle as $d = \frac{value}{2^{bitwidth(value)}}$, the PWM function is to
 *  generate a single bit output signal that is asserted $d \cdot 100\% $ of the time. The given 1MHz system clock can be used, no
 *  extra clock is required.
 *
 * @param value The desired duty cycle, encoded as an integer value.
 */
Bit pwm(UInt value)
{

	// ...

	return 'x';
}

int main()
{
	DesignScope design;

	if (true) {
		auto device = std::make_unique<IntelDevice>();
		device->setupCyclone10();
		design.setTargetTechnology(std::move(device));
	}

	// Build circuit
	Clock clock{{.absoluteFrequency = 1'000'000}}; // 1MHz
	ClockScope clockScope{ clock };

	UInt value = pinIn(4_b).setName("value");
	
	Bit ledOn = pwm(value);

	pinOut(ledOn).setName("led");

	design.postprocess();

	// Setup simulation
	sim::ReferenceSimulator simulator;
	simulator.compileProgram(design.getCircuit());

	simulator.addSimulationProcess([=]()->SimProcess{

		
		auto checkAverage = [](auto &pin, size_t duration, const Clock &clock)-> SimFunction<float> {
			size_t numerator = 0;
			for ([[maybe_unused]] auto i : Range(duration)) {
				co_await OnClk(clock);
				if (!simu(pin).allDefined()) std::cerr << "Error: Output pin is undefined, check waveform at " << toNanoseconds(getCurrentSimulationTime()) << " ns." << std::endl;
				if (simu(pin) == '1')
					numerator++;
			}
			co_return (float) numerator / duration;
		};


		simu(value) = 0;

		co_await OnClk(clock);

		for (auto i : {0, 1, 8, 10, 15}) {
			std::cout << "Starting test with value " << i << " at " << toNanoseconds(getCurrentSimulationTime()) << " ns." << std::endl;
			simu(value) = i;
			// We don't define how quickly the PWM should adapt, so be generous with the latency.
			for ([[maybe_unused]] auto j : Range(32))
				co_await OnClk(clock);

			float avg = co_await checkAverage(ledOn, 200, clock);
			float expectedAvg = (float)i / (1 << value.size());

			std::cout << "Measured average " << avg << " expected " << expectedAvg << std::endl;
			if (std::abs(avg - expectedAvg) > 0.1)
				std::cerr << "Difference exceeds expectations at " << toNanoseconds(getCurrentSimulationTime()) << " ns." << std::endl;
		}
	});


	// Record simulation waveforms as VCD file
	sim::VCDSink vcd(design.getCircuit(), simulator, "waveform.vcd");
	vcd.addAllPins();
	vcd.addAllSignals();


	if (true) {
		// VHDL export
		VHDLExport vhdl("vhdl/");
		vhdl.targetSynthesisTool(new IntelQuartus());
		vhdl.writeProjectFile("import_IPCore.tcl");
		vhdl.writeStandAloneProjectFile("IPCore.qsf");
		vhdl.writeConstraintsFile("constraints.sdc");
		vhdl.writeClocksFile("clocks.sdc");
		vhdl(design.getCircuit());
	}

	// Run simulation
	simulator.powerOn();
	simulator.advance(hlim::ClockRational(2000,1'000'000));

	return 0;
}