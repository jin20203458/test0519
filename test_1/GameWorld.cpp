#include "GameWorld.h"
#include <iostream>
#include <print>

GameWorld::GameWorld() : listenSock(INVALID_SOCKET), iocp(NULL), running(false)
{
	InitializeCriticalSection(&playersCriticalSection);
	mapPtr = new Map(-32, 2, -2, 10); 

	// MovingTrap �ʱ�ȭ
	traps.emplace_back(0.0f, 0.0f, *mapPtr, "T1");
	traps.emplace_back(3.0f, 0.0f, *mapPtr, "T2");
	traps.emplace_back(3.0f, 0.0f, *mapPtr, "T3");
	traps.emplace_back(3.0f, 0.0f, *mapPtr, "T4");
	traps.emplace_back(3.0f, 0.0f, *mapPtr, "T5");

}

GameWorld::~GameWorld() { 
	stop();
	DeleteCriticalSection(&playersCriticalSection);
}

void GameWorld::start()
{
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		std::cerr << "WSAStartup failed!\n";
		exit(-1);
	}

	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		std::cerr << "Server socket creation failed!\n";
		exit(-1);
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(PORT);

	if (bind(listenSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Bind failed!\n";
		exit(-1);
	}

	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed!\n";
		exit(-1);
	}

	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp == NULL) {
		std::cerr << "CreateIoCompletionPort failed!\n";
		exit(-1);
	}

	running = true;

	for (int i = 0; i < 4; ++i) // �񵿱� ����
		workers.emplace_back(&GameWorld::workerThread, this);
	for (auto& worker : workers) worker.detach();
	                       
	// ���� �۽� (�ֱ���)
	sendThread = std::thread(&GameWorld::sendWorldData, this);
	sendThread.detach();

	// ���� �ൿ �ֱ��� ������Ʈ
	bossThread = std::thread(&GameWorld::updateBossLoop, this);
	bossThread.detach();
	// �� �ֱ��� ������Ʈ
	mapThread = std::thread(&GameWorld::updateMapLoop, this);
	mapThread.detach();

	acceptConnections();
}

void GameWorld::acceptConnections()
{
	sockaddr_in caddr;
	SOCKET csock;
	int addrlen = sizeof(caddr);

	while (running)
	{
		csock = accept(listenSock, (SOCKADDR*)&caddr, &addrlen);
		if (csock == INVALID_SOCKET)
		{
			std::cerr << "accept failed!\n";
			continue;
		}

		// Ŭ���̾�Ʈ ���� ����
		PlayerData* newPlayer = new PlayerData("UninitPlayer", csock);

		// IOCP�� Ŭ���̾�Ʈ ���� �߰�
		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(csock), iocp,
			reinterpret_cast<ULONG_PTR>(newPlayer), 0) == NULL)
		{
			std::cerr << "CreateIoCompletionPort failed!\n";
			closesocket(csock);
			delete newPlayer;
			continue;
		}

		// Ŭ���̾�Ʈ ������ GameWorld�� �߰�
		addPlayer(csock, newPlayer);

		// ������ ������ ����
		newPlayer->session.PostRecv();
	}
}

void GameWorld::workerThread()
{
	DWORD bytesTransferred;
	ULONG_PTR completionKey;
	LPOVERLAPPED lpOverlapped;

	while (running)
	{
		BOOL result = GetQueuedCompletionStatus
		(iocp, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE);

		PlayerData* player = reinterpret_cast<PlayerData*>(completionKey);

		if (!result)              // ��������
		{
			std::cerr << "GetQueuedCompletionStatus failed! Error: " << GetLastError() << '\n';
			std::print("player {} is disconnected!\n", player->name);
			closesocket(player->session.getClientSocket());
			removePlayer(player->session.getClientSocket());
			continue;
		}
		if (bytesTransferred == 0) // ��������
		{
			std::print("player {} is disconnected!\n", player->name);
			closesocket(player->session.getClientSocket());
			removePlayer(player->session.getClientSocket());
			continue;
		}
		player->PlayerCommitWrite(bytesTransferred);

		// Ŭ���̾�Ʈ���� �����͸� ���� �� ó��
		player->PlayerPostRecv();

		// ��Ŷ�� �����Ͽ� ó��
		Packet packet;
	
		while (player->PlayerExtractPacket(packet))
		{
			// ��Ŷ Ÿ���� �а� ó��
			PacketType dataType = packet.header.type;

			if     (dataType == PacketType::PlayerInit)   player->processInit(packet);
			else if(dataType == PacketType::PlayerUpdate) player->processUpdate(packet);
			else if(dataType == PacketType::MonsterUpdate)processMonsterUpdate(packet);
			else    std::cerr << "Invalid packet type\n";
		}
	}
}


void GameWorld::processMonsterUpdate(Packet& packet)
{
	int damage = packet.read<int>();
	boss.takeDamage(damage);
}

void GameWorld::updateBossLoop()
{
	while (running)
	{
		boss.lock();
		boss.update();
		boss.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(20));  // 100ms���� �ݺ�
	}
}

void GameWorld::updateMovingTraps()
{
	for (auto& trap : traps) 
	{
		trap.update();
		/*std::print("Trap {} is at ({:.2f}, {:.2f})\n",
			trap.getId(), trap.getX(), trap.getY());*/
	}
}

void GameWorld::updateMapLoop()
{
	while (running)
	{
		updateMovingTraps(); // �� ������Ʈ
		std::this_thread::sleep_for(std::chrono::milliseconds(20));  // 100ms���� �ݺ�
	}
}

void GameWorld::sendWorldData() // ���� ������
{
	BossState currentState;

	while (running)
	{
		if (players.empty()) continue;

		// ���� ��� ����
		Packet worldPacket;
		worldPacket.header.type = PacketType::WorldUpdate;
		worldPacket.header.playerCount = players.size();
		worldPacket.header.bossActed = false;

		// �÷��̾� ������ ����ȭ
		lockPlayers();
		for (auto& pair : players)
		{
			PlayerData* player = pair.second;
			worldPacket.writeString(player->getName());   // �÷��̾� �̸�
			worldPacket.write<float>(player->getPosX());  // X ��ǥ
			worldPacket.write<float>(player->getPosY());  // Y ��ǥ
			worldPacket.write<uint8_t>(player->getAnimTypeAsByte());
		}
		unlockPlayers();

		//Boss2Phase�� true�� ���� Ʈ�� ���� ����
		if (boss.Boss2Phase)
		{
			worldPacket.write<uint8_t>(traps.size()); // Ʈ�� ����
			for (const auto& trap : traps)
			{
				worldPacket.writeString(trap.getId());        // Ʈ�� ID
				worldPacket.write<float>(trap.getX());        // Ʈ�� X ��ǥ
				worldPacket.write<float>(trap.getY());        // Ʈ�� Y ��ǥ
			}
		}
		else
		{
			// Boss2Phase�� false�� Ʈ�� ������ 0���� ��������� ������
			worldPacket.write<uint8_t>(0);
		}



		boss.lock();
		currentState = boss.getState();

		// ���� ���°� ����Ǿ����� Ȯ��
		if (boss.hasStateChanged())
		{
			std::print("BOSS state changed to {}\n", static_cast<int>(currentState));
			worldPacket.header.bossActed = true;
		}
		if (boss.hasHpChanged())
		{
			std::print("BOSS HP changed to {}\n", boss.hp);
			worldPacket.header.bossActed = true;
		}

		// ���� ������ ����ȭ
		if (worldPacket.header.bossActed) // ������ ���������� �־��ٸ�
		{
			worldPacket.write<uint8_t>(static_cast<uint8_t>(currentState)); // ���� ����
			worldPacket.write<int>(boss.hp); // ���� HP
		}
		boss.unlock();


		// ���� ��Ŷ ����ȭ
		std::vector<uint8_t> serializedPacket = worldPacket.Serialize();  
		const char* sendBuffer = reinterpret_cast<const char*>(serializedPacket.data());

		// ����ȭ�� ���� �����͸� ��� �÷��̾�� ��ε�ĳ��Ʈ
		for (auto& pair : players)
		{
			PlayerData* player = pair.second;
			int bytesSent = send(player->getClientSession().getClientSocket(), sendBuffer
				, static_cast<int>(serializedPacket.size()), 0);
		}
		//std::cout << "��Ŷ ������" << serializedPacket.size() << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));  // ������ �ӵ� ����(�������� Ŭ���̾�Ʈ���)
	}
}


void GameWorld::addPlayer(SOCKET socket, PlayerData* player)
{
	lockPlayers();
	players[socket] = player;
	unlockPlayers();
}

void GameWorld::removePlayer(SOCKET socket)
{
	lockPlayers();
	auto it = players.find(socket);
	if (it != players.end())
	{
		delete it->second;
		players.erase(it);
	}
	unlockPlayers();
}

PlayerData* GameWorld::getPlayer(SOCKET socket)
{
	lockPlayers();
	auto it = players.find(socket);
	PlayerData* player = nullptr;
	if (it != players.end())
	{
		player = it->second;
	}
	unlockPlayers();
	return player;
}

void GameWorld::lockPlayers()
{
	EnterCriticalSection(&playersCriticalSection);
}

void GameWorld::unlockPlayers()
{
	LeaveCriticalSection(&playersCriticalSection);
}

void GameWorld::stop()
{
	running = false;
	closesocket(listenSock);
	for (auto& pair : players)
	{
		closesocket(pair.first);
		delete pair.second;
	}
	players.clear();
	WSACleanup();
}