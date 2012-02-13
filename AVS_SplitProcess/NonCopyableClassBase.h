#pragma once

class NonCopyableClassBase
{
public:
    NonCopyableClassBase() {};
private:
    NonCopyableClassBase(const NonCopyableClassBase&);
    NonCopyableClassBase& operator=(const NonCopyableClassBase&);
};