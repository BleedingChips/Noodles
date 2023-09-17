import std;
import PotatoTaskSystem;
import NoodlesContext;


using namespace Noodles;

int main()
{
	auto TSystem = Potato::Task::TaskContext::Create();
	TSystem->FireThreads();

	auto NContext = Context::Create(TSystem);

	EntityPolicy Policy;

	auto Entity = NContext->CreateEntity(Policy);

	return 0;
}