#version 450

layout(binding = 0) uniform sampler   s_sampler;
layout(binding = 1) uniform texture2D t_texture;

layout(binding = 2) uniform sampler   s_gamma;
layout(binding = 3) uniform texture1D t_gamma;

layout(location = 0) in  vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

void main() {
  vec4 color = texture(sampler2D(t_texture, s_sampler), i_texcoord);

  o_color = vec4(
    texture(sampler1D(t_gamma, s_gamma), color.r).r,
    texture(sampler1D(t_gamma, s_gamma), color.g).g,
    texture(sampler1D(t_gamma, s_gamma), color.b).b,
    color.a);
}