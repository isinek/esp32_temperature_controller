compile:
	arduino-cli compile \
		--config-file ~/.arduino15/arduino-cli.yaml \
		--fqbn esp32:esp32:XIAO_ESP32C3 \
		./

test:
	g++ \
		-std=c++17 \
		-Wall \
		-Wextra \
		-pedantic \
		tests/temperature_logic_test.cpp \
		-o /tmp/temperature_logic_test
	/tmp/temperature_logic_test

upload:
	arduino-cli upload \
		--config-file ~/.arduino15/arduino-cli.yaml \
		-p /dev/ttyACM0 \
		--fqbn esp32:esp32:XIAO_ESP32C3 \
		./

monitor:
	arduino-cli monitor \
	    -p /dev/ttyACM0 \
	    --config baudrate=115200
