#pragma once

#include <cstdint>

class ShaderBase {
protected:
	unsigned int program = 0;
public:
	ShaderBase(const char* vtx_shader, const char* frag_shader);
	virtual ~ShaderBase() {}
	void use() noexcept;
};


class BlurShader : public ShaderBase
{
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

	static constexpr const char* frag_shader2 = R"(
			#version 330 core
			uniform sampler2D Texture;

			in vec2 Frag_UV;
			in vec4 Frag_Color;
			uniform mat4 ProjMtx;

			uniform vec2 Resolution;
			uniform float Time;

			out vec4 Out_Color;

	)";

	static constexpr const char* frag_shader = R"(
			#version 330 core
			uniform sampler2D Texture;

			in vec2 Frag_UV;
			in vec4 Frag_Color;
			uniform mat4 ProjMtx;

			uniform vec2 Resolution;
			uniform float Time;

			out vec4 Out_Color;
			void main()	{
				//Out_Color = vec4(1.f, 0.f, 0.f, 1.f);

				float Pi = 6.28318530718; // Pi*2
    
				// GAUSSIAN BLUR SETTINGS {{{
				float Directions = 32.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
				float Quality = 4.0; // BLUR QUALITY (Default 4.0 - More is better but slower)
				float Size = 64.0; // BLUR SIZE (Radius)
				// GAUSSIAN BLUR SETTINGS }}}
   
				vec2 Radius = Size/Resolution.xy;
    
				// Normalized pixel coordinates (from 0 to 1)
				vec2 uv = Frag_UV;
				// Pixel colour
				vec4 Color = texture(Texture, uv);
    
				// Blur calculations
				for( float d=0.0; d<Pi; d+=Pi/Directions)
				{
					for(float i=1.0/Quality; i<=1.0; i+=1.0/Quality)
					{
						Color += texture(Texture, uv+vec2(cos(d),sin(d))*Radius*i);		
					}
				}
    
				// Output to screen
				Color /= Quality * Directions - 15.0;

				float l = (0.299*Color.r + 0.587*Color.g + 0.114*Color.b);
				Out_Color = vec4(l, l, l, 1.f);
				//Out_Color = Color;
			}
	)";

public:
	BlurShader()
		: ShaderBase(vtx_shader, frag_shader)
	{}
	void ProjMtx(const float* mat4) noexcept;
	void Resolution(const float* vec2) noexcept;
	void Time(float time) noexcept;
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
			void main()	{
				Frag_UV = UV;
				Frag_Color = Color;
				gl_Position = ProjMtx * vec4(Position.xy,0,1);
			}
		)";

	// shader from https://www.shadertoy.com/view/4lK3DK
	static constexpr const char* frag_shader2 = R"(

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

			void main()	{
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

	int32_t ProjMtxLoc = 0;
	int32_t RotationLoc = 0;
	int32_t ZoomLoc = 0;
	int32_t AspectLoc = 0;
	int32_t VideoAspectLoc = 0;

	// this shader handles SBS + top/bottom 180 & top/bottom 360
	// SBS 360 is untested
	static constexpr const char* frag_shader = R"(
			#version 330 core
			uniform sampler2D Texture;
			uniform vec2 rotation;
			uniform float zoom;
			uniform float aspect_ratio;
			uniform float video_aspect_ratio;

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

			float map(float value, float min1, float max1, float min2, float max2) {
			  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
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
				vec2 texCoord = vec2(atan(rd.z, rd.x) + PI, acos(-rd.y)) / vec2(2.0f * PI, PI);
				if(video_aspect_ratio <= 1.f) {
					texCoord.y = map(texCoord.y, 0.0f, 1.0f, 0.0f, 0.5f);
				}
				Out_Color = texture(Texture, texCoord);
			}
	)";

	void InitUniformLocations() noexcept;
public:
	VrShader() 
		: ShaderBase(vtx_shader, frag_shader) 
	{
		InitUniformLocations();
	}

	void ProjMtx(const float* mat4) noexcept;
	void Rotation(const float* vec2) noexcept;
	void Zoom(float zoom) noexcept;
	void VideoAspectRatio(float aspect) noexcept;
	void AspectRatio(float aspect) noexcept;
};


class WaveformShader : public ShaderBase
{
private:
	static constexpr const char* vtx_shader = R"(
			#version 330 core
			uniform mat4 ProjMtx;
			in vec2 Position;
			in vec2 UV;
			in vec4 Color;
			out vec2 Frag_UV;
			out vec4 Frag_Color;
			void main()	{
				Frag_UV = UV;
				Frag_Color = Color;
				gl_Position = ProjMtx * vec4(Position.xy,0,1);
			}
	)";

	int32_t ProjMtxLoc = 0;
	int32_t AudioLoc = 0;
	int32_t AudioScaleLoc = 0;
	int32_t AudioSamplingOffset = 0;
	int32_t ColorLoc = 0;

	static constexpr const char* frag_shader = R"(
			#version 330 core
			uniform vec3 Color;
			uniform sampler1D audio;
			uniform float scaleAudio;
			uniform float SamplingOffset;

			in vec2 Frag_UV;
			in vec4 Frag_Color;

			out vec4 Out_Color;

			float map(float value, float min1, float max1, float min2, float max2) {
			  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
			}

			// https://shahriyarshahrabi.medium.com/procedural-color-algorithm-a37739f6dc1
			#define _Color1 vec3(0.16470588235, 0.61568627451, 0.56078431372) 
			#define _Color2 vec3(0.91372549019, 0.76862745098, 0.41568627451)
			//#define _Color3 vec3(0.95686274509, 0.63529411764, 0.38039215686)
			#define _Color3 Color

			vec3 sampleOnATriangle(float r1, float r2 ){
				return (1. - sqrt(r1))*_Color1 + (sqrt(r1)*(1. - r2))*_Color2 + (r2*sqrt(r1)) * _Color3;   
			}

			float randOneD(float seed){
				return fract(sin(seed*21.)*61.);
			}

			void main()	{
				const float frequencyBase = 16000.f;
				const float lowT = (500.f / frequencyBase) * 2.f;
				const float midT = (2000.f / frequencyBase) * 2.f;

				float unscaledSample = texture(audio, Frag_UV.x + SamplingOffset).x;
				float sample = unscaledSample * scaleAudio;
				float padding = (1.f - sample) / 2.f;
				
				//float h1 = step(0.f, (sample/2.f) - abs(Frag_UV.y - 0.5f));
				//float m1 = step(midT, (sample/2.f) - abs(Frag_UV.y - 0.5f));
				//float l1 = step(lowT, (sample/2.f) - abs(Frag_UV.y - 0.5f));
				
				float normPos = (sample/2.f) - abs(Frag_UV.y - 0.5f);
				float h1 = step(0.f, normPos);
				float m1 = smoothstep(lowT, midT, normPos);
				float l1 = smoothstep(0.f, lowT, normPos);
				float s1 = smoothstep(-0.04f, 0.0f, normPos);

				vec3 highCol = sampleOnATriangle(Color.x + Color.y, Color.x + Color.z);
				vec3 midCol = sampleOnATriangle(Color.y + Color.z, Color.y + Color.x);
				vec3 lowCol = sampleOnATriangle(Color.z + Color.x, Color.z + Color.y);

				vec3 c = mix(highCol, midCol, l1);
				c = mix(c, lowCol, m1);
				Out_Color = vec4(c, h1 + s1);
			}
	)";

	void initUniformLocations() noexcept;
public:
	WaveformShader()
		: ShaderBase(vtx_shader, frag_shader)
	{
		initUniformLocations();
	}

	void ProjMtx(const float* mat4) noexcept;
	void AudioData(uint32_t unit) noexcept;
	void SampleOffset(float offset) noexcept;
	void ScaleFactor(float scale) noexcept;
	void Color(float* vec3) noexcept;
};

class LightingShader : public ShaderBase
{
private:
	static constexpr const char* vtx_shader = R"(
		#version 330 core
		layout (location = 0) in vec3 aPos;
		layout (location = 1) in vec3 aNormal;

		out vec3 FragPos;
		out vec3 Normal;

		uniform mat4 model;
		uniform mat4 view;
		uniform mat4 projection;

		void main() {
			FragPos = vec3(model * vec4(aPos, 1.0));
			Normal = mat3(transpose(inverse(model))) * aNormal;  
    
			gl_Position = projection * view * vec4(FragPos, 1.0);
		}
	)";
	int32_t ModelLoc = 0;
	int32_t ViewLoc = 0;
	int32_t ProjectionLoc = 0;
	int32_t LightPosLoc = 0;
	int32_t ViewPosLoc = 0;
	int32_t ObjectColorLoc = 0;
	
	static constexpr const char* frag_shader = R"(
		#version 330 core
		out vec4 FragColor;

		in vec3 Normal;  
		in vec3 FragPos;  
  
		uniform vec3 lightPos; 
		uniform vec3 viewPos; 
		uniform vec3 lightColor;
		uniform vec4 objectColor;

		void main() {
			// ambient
			float ambientStrength = 0.1;
			vec3 ambient = ambientStrength * lightColor;
  	
			// diffuse 
			vec3 norm = normalize(Normal);
			vec3 lightDir = normalize(lightPos - FragPos);
			float diff = max(dot(norm, lightDir), 0.0);
			vec3 diffuse = diff * lightColor;
    
			// specular
			float specularStrength = 0.5;
			vec3 viewDir = normalize(viewPos - FragPos);
			vec3 reflectDir = reflect(-lightDir, norm);  
			float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
			vec3 specular = specularStrength * spec * lightColor;  
        
			vec4 result = vec4(ambient + diffuse + specular, 1.f) * objectColor;
			FragColor = result;
		} 
	)";
	void initUniformLocations() noexcept;
public:
	LightingShader() noexcept : ShaderBase(vtx_shader, frag_shader)
	{
		initUniformLocations();
	}

	void ModelMtx(const float* mat4) noexcept;
	void ProjectionMtx(const float* mat4) noexcept;
	void ViewMtx(const float* mat4) noexcept;
	void ObjectColor(const float* vec4) noexcept;
	void LightPos(const float* vec3) noexcept;
	void ViewPos(const float* vec3) noexcept;
};