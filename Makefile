compile:
	arduino-cli compile \
		--config-file ~/.arduino15/arduino-cli.yaml \
		--fqbn esp32:esp32:XIAO_ESP32C3 \
		./

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
