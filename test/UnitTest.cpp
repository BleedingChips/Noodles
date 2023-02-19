#include "Noodles/NoodlesMemory.h"
#include "Noodles/NoodlesTypeGroup.h"
#include <chrono>
#include <iostream>

int main()
{

	{
		Noodles::Memory::ChunkManager::Chunk::Ptr P;
		{
			auto Manager = Noodles::Memory::ChunkManager::CreateInstance();
			for (std::size_t I = 0; I < 1000; ++I)
			{
				auto K = Manager->TryAllocate(10);
				P = K;
				//std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
				//std::cout << reinterpret_cast<std::size_t>(K->GetBuffer().data()) << std::endl;
			}
		}
		volatile int i = 0;
	}
	
	

	//auto Ptr2 = Noodles::Memory::ChunkManager::Create();

	return 0;
}