all	:
	cd LobbyServer && make cl && make -j8 && cd ..
	cd GateServer && make cl && make -j8 && cd ..

cl	:
	cd LobbyServer && make cl && cd ..
	cd GateServer && make cl && cd ..

release	:
	cd LobbyServer && make cl && make release && cd ..
	cd GateServer && make cl && make release && cd ..

db	:
	cd LobbyServer && make cl && compiledb -n make && cd ..
	cd GateServer && make cl && compiledb -n make && cd ..

pack	: all copy

copy	:
	cp LobbyServer/LobbyServer.out bin/
	cp GateServer/GateServer.out bin/
