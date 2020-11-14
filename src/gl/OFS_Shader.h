#pragma once

class ShaderBase {
protected:
	unsigned int program = 0;
public:
	ShaderBase(const char* vtx_shader, const char* frag_shader);
	virtual ~ShaderBase() {}
	void use() noexcept;
};



class VrShader : public ShaderBase {
private:
	static constexpr const char* vtx_shader = R"(
			#version 330 core
			uniform mat4 ProjMtx;
			in vec2 Position;
			in vec2 UV;
			in vec4 Color;
			out vec2 Frag_UV;
			out vec4 Frag_Color;
			void main()
			{
				Frag_UV = UV;
				Frag_Color = Color;
				gl_Position = ProjMtx * vec4(Position.xy,0,1);
			}
		)";

	// shader from https://www.shadertoy.com/view/4lK3DK
	static constexpr const char* frag_shader = R"(

			#version 330 core
			uniform sampler2D Texture;
			uniform vec2 rotation;
			uniform float zoom;
			uniform float aspect_ratio;

			in vec2 Frag_UV;
			in vec4 Frag_Color;

			out vec4 Out_Color;
			#define PI 3.1415926535
			#define DEG2RAD 0.01745329251994329576923690768489
		
			float hfovDegrees = 75.0;
			float vfovDegrees = 59.0;

			vec3 rotateXY(vec3 p, vec2 angle) {
				vec2 c = cos(angle), s = sin(angle);
				p = vec3(p.x, c.x*p.y + s.x*p.z, -s.x*p.y + c.x*p.z);
				return vec3(c.y*p.x + s.y*p.z, p.y, -s.y*p.x + c.y*p.z);
			}

			void main()
			{
				float inverse_aspect = 1.f / aspect_ratio;
				float hfovRad = hfovDegrees * DEG2RAD;
				float vfovRad = -2.f * atan(tan(hfovRad/2.f)*inverse_aspect);

				vec2 uv = vec2(Frag_UV.s - 0.5, Frag_UV.t - 0.5);

				//to spherical
				vec3 camDir = normalize(vec3(uv.xy * vec2(tan(0.5 * hfovRad), tan(0.5 * vfovRad)), zoom));
				//camRot is angle vec in rad
				vec3 camRot = vec3( (rotation - 0.5) * vec2(2.0 * PI,  PI), 0.);

				//rotate
				vec3 rd = normalize(rotateXY(camDir, camRot.yx));

				//radial azmuth polar
				vec2 texCoord = vec2(atan(rd.z, rd.x) + PI, acos(-rd.y)) / vec2(2.0 * PI, PI);

				Out_Color = texture(Texture, texCoord);
			}
	)";
public:
	VrShader() 
		: ShaderBase(vtx_shader, frag_shader) 
	{}

	void ProjMtx(const float* mat4) noexcept;
	void Rotation(const float* vec2) noexcept;
	void Zoom(float zoom) noexcept;
	void AspectRatio(float aspect) noexcept;
};