#pragma once
#include "Map.h"
#include <print>
#include <cmath>
#include <random>
#include <string>

constexpr float TRAP_SPEED = 0.2f; // Ʈ�� �ӵ�
class MovingTrap {
public:
    MovingTrap(float x, float y, const Map& map, const std::string& id)
        : x(x), y(y), speed(TRAP_SPEED), mapRef(map), id(id)
    {
        setRandomDirection();
        TrapHP = 40.0f; // �ʱ� HP ����
    }

    void update()
    {
        float nextX = x + dirX * speed;
        float nextY = y + dirY * speed;

        // �̵� �� �� ��ȿ ���� Ȯ��
        if (mapRef.isValidPosition(static_cast<int>(nextX), static_cast<int>(nextY)))
        {
            x = nextX;
            y = nextY;
        }
        else {
            setRandomDirection(); // ��ȿ ������ ����� ���� �ٽ� ����
        }
    }

    bool takeDamage(float amount)
    {
        TrapHP -= amount;
		std::print("Trap {} took damage: {}. Remaining HP: {}\n", id, amount, TrapHP);  
        return TrapHP <= 0.0f;
    }

    float getTrapHP() const { return TrapHP; } // Ʈ���� HP ��ȯ
    float getX() const { return x; }
    float getY() const { return y; }
    const std::string& getId() const { return id; } // �ĺ��� ��ȯ

private:
    float x, y;
    float dirX, dirY; // ���� ���� ����
    float speed;
    const Map& mapRef;
    std::string id;   // ���� �ĺ���
    float TrapHP;     // Ʈ���� HP
    void setRandomDirection()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> offset(-3.0f, 3.0f); // �ֺ� 3ĭ ����

        // ������ ��ġ ����
        float tx = x + offset(gen);
        float ty = y + offset(gen);

        // ���� ���� ���� ���
        float vx = tx - x;
        float vy = ty - y;
        float mag = std::sqrt(vx * vx + vy * vy);

        // ���� ���� ���� ����ȭ
        if (mag != 0.0f)
        {
            dirX = vx / mag;
            dirY = vy / mag;
        }
        else  // 0���� ������ ��� ����
        {
            dirX = 1.0f;
            dirY = 0.0f;
        }
    }

};
