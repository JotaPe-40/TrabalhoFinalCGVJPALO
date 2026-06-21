#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da posição global e a normal de cada vértice, definidas em
// "shader_vertex.glsl" e "main.cpp".
in vec4 position_world;
in vec4 normal;

// Posição do vértice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
in vec2 texcoords;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificador que define qual objeto está sendo desenhado no momento
#define SPHERE 0
#define BUNNY  1
#define PLANE  2
#define WALL   3
#define TETO   4
#define RAT    5
uniform int object_id;

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// ===========================================================================
// Iluminação: a ÚNICA fonte de luz da cena é a lanterna do personagem. Sua
// posição (em coordenadas de mundo) é recalculada a cada quadro em main()
// (função ComputeLanternPosition()) a partir da posição/orientação atual
// do jogador, e enviada aqui.
//
// "light_occluded" é decidido individualmente para cada objeto desenhado
// (cada parede, cada rato, o smile), em main.cpp, testando se alguma
// parede do labirinto bloqueia a linha reta entre a lanterna e aquele
// objeto (função LightReachesPoint(), que reaproveita MazeCollides()).
// Quando ocluso, a luz não atinge o objeto, fazendo as paredes do
// labirinto efetivamente "barrarem" a luz da lanterna, como num jogo de
// terror/exploração: o labirinto fica escuro, exceto onde o personagem,
// com sua lanterna, ilumina.
// ===========================================================================
uniform vec4 light_position;
uniform int light_occluded;

// Variáveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;
uniform sampler2D TextureImage3;
uniform sampler2D TextureImage4;
uniform float TextureRepeat;

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec4 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

void main()
{
    // Obtemos a posição da câmera utilizando a inversa da matriz que define o
    // sistema de coordenadas da câmera.
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    // O fragmento atual é coberto por um ponto que percente à superfície de um
    // dos objetos virtuais da cena. Este ponto, p, possui uma posição no
    // sistema de coordenadas global (World coordinates). Esta posição é obtida
    // através da interpolação, feita pelo rasterizador, da posição de cada
    // vértice.
    vec4 p = position_world;

    // Normal do fragmento atual, interpolada pelo rasterizador a partir das
    // normais de cada vértice.
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz (a lanterna do personagem)
    // em relação ao ponto atual.
    vec4 l = normalize(light_position - p);

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Distância entre a lanterna e o ponto atual, usada para a atenuação
    // da luz com a distância (uma lanterna real ilumina menos quanto mais
    // longe o objeto está, e tem um alcance limitado).
    float light_distance = length(light_position - p);

    // Coordenadas de textura U e V
    float U = 0.0;
    float V = 0.0;

    //sentido da reflexão especular
    vec4 r = normalize(-l + 2*n*dot(n,l));

	// Coeficiente de refletância difusa
	vec3 Kd0;
    vec3 Ks; // Refletância especular
    vec3 Ka; // Refletância ambiente
    float q; // Expoente especular para o modelo de iluminação de Phong
    vec3 Ke = vec3(0.0,0.0,0.0); // Emissão própria (luz própria do objeto, independente de qualquer fonte de luz externa)

    if ( object_id == SPHERE )
    {
        // Coordenadas de textura do "smile", computadas com projeção
        // esférica em coordenadas do modelo (a esfera de projeção está
        // centrada em "bbox_center", o centro da bounding box do objeto).
        vec4 bbox_center = (bbox_min + bbox_max) / 2.0;
        vec4 d = position_model - bbox_center;

        float rho   = length(d);
        float theta = atan(d.x,d.z);
        float phi   = asin(d.y / rho);

        U = (theta + M_PI) / 2.0 / M_PI;
        V = (phi + M_PI_2) / M_PI;

		// Obtemos a refletância difusa a partir da leitura da imagem TextureImage3
		Kd0 = texture(TextureImage3, vec2(U,V)).rgb;
        // O smile é um objeto "auto-iluminado" (como se brilhasse com luz
        // própria, marcando visualmente o objetivo do jogador mesmo a
        // distância, inclusive nas partes na sombra ou ocluídas por
        // paredes): usamos um termo de EMISSÃO PRÓPRIA (Ke), que é somado
        // à cor final independente de qualquer luz externa - diferente de
        // Ka, que só tem efeito multiplicado pela luz ambiente da cena
        // (Ia), que é deliberadamente bem baixa para o resto do labirinto
        // ficar escuro fora do alcance da lanterna.
        Ks = vec3(0.3,0.3,0.2);
        Ka = vec3(0.10,0.10,0.06);
        Ke = Kd0 * 0.55;
        q = 32.0;

    }
    else if ( object_id == BUNNY )
    {
        float minx = bbox_min.x;
        float maxx = bbox_max.x;

        float miny = bbox_min.y;
        float maxy = bbox_max.y;

        U = (position_model.x - minx) / (maxx - minx);
        V = (position_model.y - miny) / (maxy - miny);

		// Obtemos a refletância difusa a partir da leitura da imagem TextureImage3
		Kd0 = texture(TextureImage3, vec2(U,V)).rgb;
        Ks = vec3(0.0,0.0,0.0);
        Ka = Kd0 * 0.15;
        q = 1.0;
    }
    else if ( object_id == PLANE )
    {
        vec2 uv = texcoords * TextureRepeat;
        Kd0 = texture(TextureImage1, uv).rgb;
        Ks = vec3(0.0,0.0,0.0);
        // Refletância ambiente baixa, proporcional à própria textura: o
        // chão longe da lanterna fica bem escuro (quase preto), mas ainda
        // com um resquício de cor para não desaparecer completamente.
        Ka = Kd0 * 0.06;
        q = 1.0;
    }
    else if ( object_id == TETO ) {
         vec2 uv = texcoords * TextureRepeat; 
         Kd0 = texture(TextureImage2, uv).rgb;
        Ks = vec3(0.0,0.0,0.0);
        Ka = Kd0 * 0.06;
        q = 1.0;
    }
    else if ( object_id == WALL )
    {
        vec2 uvw = texcoords * TextureRepeat;
        Kd0 = texture(TextureImage0, uvw).rgb;
        Ks = vec3(0.15,0.15,0.15);
        // Mesma ideia do chão/teto: as paredes ficam bem escuras fora do
        // alcance da lanterna, mas não 100% pretas (mantendo um mínimo de
        // legibilidade da geometria do labirinto).
        Ka = Kd0 * 0.08;
        q = 18.0;
    }

    else if ( object_id == RAT )
    {
        // Coordenadas de textura DO PRÓPRIO OBJETO: o modelo data/rat.obj
        // (convertido de assets/rat.stl) inclui coordenadas de textura (vt)
        // próprias, calculadas com um mapeamento cilíndrico ao redor do
        // eixo longitudinal real do corpo do rato (acompanhando a
        // curvatura da cabeça à cauda) - ao contrário da projeção planar
        // via bounding box usada para o BUNNY/PLANE/TETO/WALL, aqui as
        // coordenadas já vêm prontas do arquivo OBJ (atributo "texcoords",
        // interpolado pelo rasterizador a partir de cada vértice).
        U = texcoords.x;
        V = texcoords.y;

        Kd0 = texture(TextureImage4, vec2(U,V) * TextureRepeat).rgb;
        Ks = vec3(0.05,0.05,0.05);
        // Refletância ambiente proporcional à própria textura de pelagem:
        // evita que partes do corpo do rato cuja normal não aponte para a
        // lanterna fiquem completamente pretas (o problema original).
        Ka = Kd0 * 0.35;
        q = 4.0;
    }

    // Espectro da fonte de iluminação (a lanterna do personagem).
    vec3 I = vec3(1.4,1.3,1.0);

    // Espectro da luz ambiente global da cena: bem baixo, para que o
    // labirinto fique escuro fora do alcance da lanterna (mas as
    // contribuições de Ka/Kd0 acima ainda dão um mínimo de visibilidade).
    vec3 Ia = vec3(0.06,0.06,0.08);

    // Atenuação da luz da lanterna com a distância (modelo físico simples
    // de atenuação quadrática-linear, comum em jogos): quanto mais longe o
    // objeto está da lanterna, mais fraca a luz que o atinge. Os
    // coeficientes foram calibrados para que a luz alcance bem uma ou duas
    // células do labirinto à frente do personagem, e praticamente se
    // apague depois disso.
    float attenuation = 1.0 / (1.0 + 0.55 * light_distance + 0.40 * light_distance * light_distance);

    // Se uma parede do labirinto bloqueia a linha entre a lanterna e este
    // objeto (decidido em main.cpp, por objeto, e enviado como uniform),
    // a luz direta da lanterna não alcança o fragmento - apenas a luz
    // ambiente global (Ia) e a contribuição ambiente do próprio material
    // (Ka) continuam valendo, mantendo o objeto na penumbra, não 100% preto.
    if ( light_occluded == 1 )
    {
        attenuation = 0.0;
    }

    // Termo difuso utilizando a lei dos cossenos de Lambert, com a
    // intensidade da luz já atenuada pela distância (e pela oclusão).
    vec3 lambert_diffuse_term = Kd0 * I * attenuation * max(0,dot(n,l));

    // Termo ambiente: luz ambiente global da cena, multiplicada pela
    // refletância ambiente do material de cada objeto.
    vec3 ambient_term = Ka*Ia;

    // Termo especular utilizando o modelo de iluminação de Phong, também
    // atenuado pela distância/oclusão da lanterna.
    vec3 phong_specular_term  = Ks*I*attenuation*pow(max(0,dot(r,v)),q);

    // NOTE: Se você quiser fazer o rendering de objetos transparentes, é
    // necessário:
    // 1) Habilitar a operação de "blending" de OpenGL logo antes de realizar o
    //    desenho dos objetos transparentes, com os comandos abaixo no código C++:
    //      glEnable(GL_BLEND);
    //      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // 2) Realizar o desenho de todos objetos transparentes *após* ter desenhado
    //    todos os objetos opacos; e
    // 3) Realizar o desenho de objetos transparentes ordenados de acordo com
    //    suas distâncias para a câmera (desenhando primeiro objetos
    //    transparentes que estão mais longe da câmera).
    // Alpha default = 1 = 100% opaco = 0% transparente
    color.a = 1;

    // Cor final do fragmento calculada com uma combinação dos termos difuso,
    // especular, ambiente, e emissivo (luz própria, usada apenas pelo
    // "smile"). Veja slide 129 do documento Aula_17_e_18_Modelos_de_Iluminacao.pdf.
    color.rgb = lambert_diffuse_term + ambient_term + phong_specular_term + Ke;

    // Cor final com correção gamma, considerando monitor sRGB.
    // Veja https://en.wikipedia.org/w/index.php?title=Gamma_correction&oldid=751281772#Windows.2C_Mac.2C_sRGB_and_TV.2Fvideo_standard_gammas
    color.rgb = pow(color.rgb, vec3(1.0,1.0,1.0)/2.2);
} 
