# 切换环境

```shell
conda info --envs #or conda env list

#out
#base                  *  C:\ProgramData\Anaconda3
#spider                   C:\ProgramData\Anaconda3\envs\spider

conda activate spider   #激活
conda deactivate spider	#取消激活
```

# 更换源

```shell
conda config --add channels https://mirrors.ustc.edu.cn/anaconda/pkgs/main/
conda config --add channels https://mirrors.ustc.edu.cn/anaconda/pkgs/free/
conda config --add channels https://mirrors.ustc.edu.cn/anaconda/cloud/conda-forge/
conda config --add channels https://mirrors.ustc.edu.cn/anaconda/cloud/msys2/
conda config --add channels https://mirrors.ustc.edu.cn/anaconda/cloud/bioconda/
conda config --add channels https://mirrors.ustc.edu.cn/anaconda/cloud/menpo/
conda config --set show_channel_urls yes
```



# 禁用SSL

```she
 conda config --set ssl_verify false
```

