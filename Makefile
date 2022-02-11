CONTRACT=./arci_presale

all: $(CONTRACT).wasm $(CONTRACT).abi

%.wasm: %.cpp ./arci_presale.hpp
	eosio-cpp -I. -o $@ $<

%.abi: %.cpp
	eosio-abigen -contract=$(CONTRACT) --output=$@ $<

clean:
	rm -f $(CONTRACT).wasm $(CONTRACT).abi

push:
	cleos -u https://explorer.vexanium.com:6960 set contract arci_presale .  $(CONTRACT).wasm $(CONTRACT).abi
