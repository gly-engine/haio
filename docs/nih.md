# Not Invented Here

Pode soar meio contraditório, sitar (NIH) em apenas mais uma ferramenta de proxy ou conversão de imagem.
mas pelo contrário Haio não tem uma filosofia banal como "mesmo software só que escrito em rust" ou então "um design de interface do meu gosto pessoal", na verdade a base de apoio de sua perfomance além arquiteturalmente é justamente usar códigos maduro criado por terceiros. como jpeg-turbo, libyuv, wuffs. que são especificos para fazer uma coisa só, porém muito bem feito.

## Problems to Solve

Não é uma ferramenta surgida pelo fator vontade de criar, mas é um misto de problemas empresarias privados e também projetos pessoais que demandam pela ferramenta. ambos especificos e muito similares.

### Imagick Issues

ImageMagick é um grande software com grandes problemas, apesar de ajudar muito no dia a dia, é uma ferramenta puramente local para trabalho e nada confiavel em um ambiente de produção para seu funcionamento. além de ter uma manutenção dificil e ambiente defasado.

#### Secutiry Problems

ImageMagick tem histórico de CVEs recorrentes. Ele delega grande parte de suas funcionalidades a bibliotecas externas, aumentando a superfície de ataque e a dificuldade de controle, além de possuir uma cadeia de processamento extremamente extensa para uma tarefa aparentemente simples como converter uma imagem.

**Solution:** Haio acaba tendo maior controle e sem delegações externas utilizando sempre as versões mais recente de suas depdencias, além de ter um fluxo de processamento simplificado e direto ao ponto. e codificando usando técnicas mais seguras e modernas.

#### Runtime Heterogeneity

ImageMagick tem uma cadeia de dependencias complexas se ligando dinamicamente além de poder trazer o problemas de segurança citados anteriormente, atrapalha a portabilidade e ainda pior a homogenia de comportamento entre ambientes, o mesmo binário em maquinas diferentes pode apresentar comportamentos distintos.

**Solution:** Haio é um binário unico estaticamente linkado, sem dependencias de instalação, funcionando sempre da mesma forma indepentente da distruibuição, libc e pode rodar em um container docker from scratch, onde seu unico artefato é binário com tudo que precisa para ser executa-do.

#### Interface for Web

ImageMagick não foi naturalmente construído para ser utilizado na web. Sua integração com um SaaS exige wrappers extensos para expor manualmente suas funções à linguagem hospedeira, como PHP ou Java, ou o acionamento de sua CLI por processos externos, aumentando o acoplamento, a complexidade e os riscos de estabilidade e segurança.

**Solution:** Haio é um processo isolado que pode ser acionado via protocolo http, expondo todas as suas funcionalidaes do CLI ao seu proxy, que pode ser utilizando como serviço de borda como um CDN, ou então um processador interno de imagens para serem gravados no storage.

### Exoteric Formats

#### GPU Containers

