import std;
import NoodlesForm;

using namespace Noodles;


int main()
{
	auto man = Form::FormManager::Create();

	auto f1 = Form::Form::Create();

	man->CreateAndBindForm(*f1, {});

	return 0;
}