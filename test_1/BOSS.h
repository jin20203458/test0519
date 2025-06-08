#pragma once
#include <windows.h>
#include <cstdint>

// ������ ���� ����
enum class BossState : uint8_t 
{
    IDLE = 0x00,          // ���
    LeftFistDown = 0x01,  // �޼� ����ġ��
    RightFistDown = 0x02, // ������ ����ġ��
    AllFistDown = 0x03,   // ��� ����ġ��
	Shout = 0x04,         // ��ħ (���� Ȯ�� ����)
	LeftWield = 0x05,     // �޼� �ֵθ���
	DEAD = 0x06,         // ����  
};

class BOSS {
public:
    BOSS();
    ~BOSS();

    void update();                 // ���� ���� (3�ʸ��� ����)
    void takeDamage(int amount);   // ���� ó��
    bool isDead() const;           // �׾����� Ȯ��
    void reset();                  // �ʱ�ȭ

    // ���� ���� ����
    BossState getState() const;
    void setState(BossState newState);

	// ���� ���� ���� Ȯ��
    bool hasStateChanged();
    bool hasHpChanged();

    // ��ġ�� ����� ����
    float x, y;
    int hp;
    int maxHp;
	bool Boss2Phase = false;
    void lock() const;
    void unlock() const;
    bool tryLock();
private:
    void attack();        // �ൿ ���� (�̱���)
    void movePattern();   // �̵� ���� (�̱���)

    BossState currentState;
    DWORD lastUpdateTime; 
    
    int previousHp;
    BossState previousState;

    mutable CRITICAL_SECTION cs;
};
