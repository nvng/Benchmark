all	:
	cd LobbyServer && make cl && make j && cd ..
	cd GateServer && make cl && make j && cd ..
	cd Benchmark && make cl && make j && cd ..
	cd GameMgrServer && make cl && make j && cd ..
	cd DBServer && make cl && make j && cd ..

cl	:
	cd LobbyServer && make cl && cd ..
	cd GateServer && make cl && cd ..
	cd Benchmark && make cl && cd ..
	cd GameMgrServer && make cl && cd ..
	cd DBServer && make cl && cd ..

release	:
	cd LobbyServer && make cl && make release && cd ..
	cd GateServer && make cl && make release && cd ..
	cd Benchmark && make cl && make release && cd ..
	cd GameMgrServer && make cl && make release && cd ..
	cd DBServer && make cl && make release && cd ..

db	:
	cd LobbyServer && make cl && compiledb -n make && cd ..
	cd GateServer && make cl && compiledb -n make && cd ..
	cd Benchmark && make cl && compiledb -n make && cd ..
	cd GameMgrServer && make cl && compiledb -n make && cd ..
	cd DBServer && make cl && compiledb -n make && cd ..

pack	: all copy

copy	:
	cp LobbyServer/LobbyServer.out bin/
	cp GateServer/GateServer.out bin/
	cp GameMgrServer/GameMgrServer.out bin/
	cp DBServer/DBServer.out bin/
