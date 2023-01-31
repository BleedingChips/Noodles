#include "Noodles/NoodlesMemory.h"
#include "Noodles/NoodlesTypeGroup.h"
#include <chrono>

int main()
{
	auto Manager = Noodles::Memory::PageManager::Create();
	for (std::size_t I = 0; I < 1000; ++I)
	{
		auto K = Manager->Allocate(10);
		std::this_thread::sleep_for(std::chrono::milliseconds{100});
	}

	//auto Ptr2 = Noodles::Memory::ChunkManager::Create();

	return 0;
}