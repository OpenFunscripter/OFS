#pragma once

#include "im3d.h"
namespace OFS {
	bool Im3d_Init() noexcept;

	void Im3d_NewFrame() noexcept;
	void Im3d_EndFrame() noexcept;

	void Im3d_Shutdown() noexcept;

    void DecomposeMatrixToComponents(const float* matrix, float* translation, float* rotation, float* scale) noexcept;
}

