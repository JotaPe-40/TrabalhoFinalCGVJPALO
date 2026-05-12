# Especificação da Implementação

> [!CAUTION]
>
> - Você <ins>**não pode utilizar ferramentas de IA para escrever esta
>   especificação**</ins>

## Integrantes da dupla

- **Aluno 1 - Nome**: <mark>`João Pedro Alvarenga`</mark>
- **Aluno 1 - Cartão UFRGS**: <mark>`577252`</mark>

- **Aluno 2 - Nome**: <mark>`Lucas Kazuo Okano`</mark>
- **Aluno 2 - Cartão UFRGS**: <mark>`00216888`</mark>

## Detalhes do que será implementado

- **Título do trabalho**: <mark>`Labirinto: encontre o smile!`</mark>
- **Parágrafo curto descrevendo o que será implementado**: <mark>`O jogo consiste em um labirinto onde o jogador deve encontrar uma smile em forma de esfera. Outro objeto são os ratos que ficaram andando pelo labirinto que afetam a movimentação do jogador.`</mark>

## Especificação visual

### Vídeo - Link

> [!IMPORTANT]
>
> - Coloque aqui um link para um vídeo que mostre a aplicação gráfica
>   de referência que você vai implementar. **Sua implementação deverá
>   ser o mais parecido possível com o que é mostrado no vídeo (mais
>   detalhes abaixo).**
> - **Você não pode escolher como referência: (1) algum trabalho realizado
>   por outros alunos desta disciplina, em semestres anteriores. (2) Minecraft.**
> - Por exemplo, você pode colocar um vídeo de um jogo que você gosta,
>   e seu trabalho final será uma re-implementação do jogo.
> - O vídeo pode ser um link para YouTube, Google Drive, ou arquivo mp4 dentro
>   do próprio repositório. Mas, garanta que qualquer um tenha
>   permissão de acesso ao vídeo através deste link.

<mark>`https://www.youtube.com/watch?v=reroU16aQig`</mark>

### Vídeo - Timestamp

> [!IMPORTANT]
>
> - Coloque aqui um **intervalo de ~30 segundos** do vídeo acima, que
>   será a base de comparação para avaliar se o seu trabalho final
>   conseguiu ou não reproduzir a referência.

- **Timestamp inicial**: <mark>`0:00`</mark>
- **Timestamp final**: <mark>`0:30`</mark>

### Imagens

> [!IMPORTANT]
>
> - Coloque aqui **três imagens** capturadas do vídeo acima, que você
>   irá usar como ilustração para as explicações que vêm abaixo.

<mark>`...TrabalhoFinalCGVJPALO\assets\imagens_spec\Captura de tela 2026-05-11 110840.png
...TrabalhoFinalCGVJPALO\assets\imagens_spec\Captura de tela 2026-05-11 111333.png
...TrabalhoFinalCGVJPALO\assets\imagens_spec\Captura de tela 2026-05-11 111504.png'
</mark>

## Especificação textual

Para cada um dos requisitos abaixo (detalhados no [Enunciado do Trabalho final - Moodle](https://moodle.ufrgs.br/mod/assign/view.php?id=6018620)), escreva um parágrafo **curto** explicando como este requisito será atendido, apontando itens específicos do vídeo/imagens que você incluiu acima que atendem estes requisitos.

### Malhas poligonais complexas

<mark>`o ratinho que anda pelo labirinto sera refeito como um modelo poligonal complexo`</mark>

### Transformações geométricas controladas pelo usuário

<mark>`o controle de movimentação em primeira pessoa confere transformações geométricas controladas pelo usuário/jogador`</mark>

### Diferentes tipos de câmeras

<mark>`primeira pessoa e visualização por cima`</mark>

### Instâncias de objetos

<mark>`smile e ratos`</mark>

### Testes de intersecção

<mark>`teste de intersecção cubo-plano entre o jogador e as paredes 
teste cubo-esfera entre o jogador e o smile, teste cubo-cubo entre o jogador e o rato`</mark>

### Modelos de Iluminação em todos os objetos

<mark>`a Iluminação do smile tera componente especular com shading gouraud, 
as paredes terao componente especular com shading de phong, 
o teto e o chao terão apenas componente difuso`</mark>

### Mapeamento de texturas em todos os objetos

<mark>`rato com coordenadas de textura do objeto, smile com projeção esférica, e o restante com projeção planar`</mark>

### Movimentação com curva Bézier cúbica

<mark>`movimentação aleatória do rato`</mark>

### Animações baseadas no tempo ($\Delta t$)

<mark>`faixa de velocidade do rato independente do framerate do projeto`</mark>

## Limitações esperadas

> [!IMPORTANT]
>
> - Coloque aqui uma lista de detalhes visuais ou de interação que
>   aparecem no vídeo e/ou imagens acima, mas que você **não pretende
>   implementar** ou que você **irá implementar parcialmente**.
> - Para cada item, **explique por que** não será implementado ou por
>   que será implementado parcialmente.

<mark>`algumas texturas estarão levemente diferentes do original, o rato e o smile serão modelos 3d ao invés de imagens, os textos "opengl" encontrados em algumas paredes não serão aplicados, e os caminhos do labirinto podem estar diferentes dos omostrados no vídeo todas essas mudanças visam o foco do projeto no cumprimento da especificação do trabalho`</mark>
