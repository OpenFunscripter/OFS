#include "OFS_TCodeProducer.h"

void TCodeProducer::tick(int32_t CurrentTimeMs) noexcept
{
	for (auto& prod : producers) {
		prod.tick(CurrentTimeMs);
	}
}
