#include <cassert>
import std;
import NoodlesArchetype;
import NoodlesClassBitFlag;
import PotatoIR;

using namespace Noodles;

struct alignas(128) A{ std::size_t i = 0; };

struct alignas(64) B { std::size_t i = 1; };

struct alignas(32) C { std::size_t i = 1; };

struct alignas(32) D { std::size_t i = 1; };

struct Tot
{
	A a;
	B b;
	C c;
};

struct E
{
	std::size_t index = 1;
	using NoodlesSingletonRequire = std::size_t;
};


int main()
{

	ClassBitFlagMap manager;

	auto init_list = std::array{
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<A>::Create(), *manager.LocateOrAdd(*StructLayout::GetStatic<A>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<B>::Create(), *manager.LocateOrAdd(*StructLayout::GetStatic<B>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<C>::Create(), *manager.LocateOrAdd(*StructLayout::GetStatic<C>())}
	};

	

	auto ptr = Archetype::Create(manager.GetBitFlagContainerElementCount(), std::span(init_list));

	Tot temp;

	auto a = reinterpret_cast<A*>(
		ptr->GetMemberView(0).GetMember(1, &temp)
	);
	a->i = 10086;

	assert(temp.a.i == 10086);

	auto b = reinterpret_cast<B*>(
		ptr->GetMemberView(1).GetMember(1, &temp)
		);
	b->i = 100;
	assert(temp.b.i == 100);

	auto c = reinterpret_cast<C*>(
		ptr->GetMemberView(2).GetMember(1, &temp)
		);
	c->i = 86;
	assert(temp.c.i == 86);

	auto re_ad4 = ptr->FindMemberIndex(*manager.LocateOrAdd(*StructLayout::GetStatic<D>()));

	assert(!re_ad4);

	for (std::size_t count = 1; count <= 8; ++count)
	{
		std::size_t test_buffer = 1024 * count;
		auto predict_count = ptr->PredictElementCount(test_buffer);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(std::pmr::get_default_resource(), {alignof(void*), test_buffer });
		auto [align_buffer, suggest_count] = ptr->AlignBuffer(re.GetByte(), test_buffer);
		assert(suggest_count >= predict_count);

		std::span<A> span_a = {
			new (ptr->GetMemberView(0).GetMember(suggest_count, align_buffer)) A[suggest_count],
			suggest_count
		};

		for (auto& ite : span_a)
		{
			ite.i = 10086;
			assert((reinterpret_cast<std::size_t>(&ite) % alignof(A)) == 0);
		}

		std::span<B> span_b = {
			new (ptr->GetMemberView(1).GetMember(suggest_count, align_buffer)) B[suggest_count],
			suggest_count
		};

		for (auto& ite : span_b)
		{
			ite.i = 113;
			assert((reinterpret_cast<std::size_t>(&ite) % alignof(B)) == 0);
		}

		std::span<C> span_c = {
			new (ptr->GetMemberView(2).GetMember(suggest_count, align_buffer)) C[suggest_count],
			suggest_count
		};

		for (auto& ite : span_c)
		{
			ite.i = 110;
			assert((reinterpret_cast<std::size_t>(&ite) % alignof(C)) == 0);
		}

		for (auto& ite : span_a)
		{
			assert(ite.i == 10086);
		}
		for (auto& ite : span_b)
		{
			assert(ite.i == 113);
		}
		for (auto& ite : span_c)
		{
			assert(ite.i == 110);
		}

	}

	return 0;
}