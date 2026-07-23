## Creating PyENV

To create a virtual environment of python, we have to use pyenv or venv. In our case we are going to use `penv`. Let's assume that you have `python3` installed in your machine. Now follow these steps: [current distro: ubuntu 22.04]

1. Installing dependencies:

```bash
sudo apt update

sudo apt install -y make build-essential libssl-dev zlib1g-dev \
libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm \
libncurses5-dev libncursesw5-dev xz-utils tk-dev libffi-dev \
liblzma-dev git
```

2. Install `pyenv``

```bash
#if curl is not installed, install it and execute this again
curl https://pyenv.run | bash
```

3. Configure the shell

```bash
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bashrc
echo '[[ -d $PYENV_ROOT/bin ]] && export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bashrc
echo 'eval "$(pyenv init -)"' >> ~/.bashrc
```

4. Install Python 3.9.16 or your desired version

```bash
pyenv install 3.9.16
```

5. Create a virtual environment

```bash
# Syntax: pyenv virtualenv <version> <name_of_env>
pyenv virtualenv 3.9.16 netconf
```

6. Activate it

```bash
# go to your desired directory and activate it
pyenv local netconf
pyenv activate netconf
```

## Python Dependencies

Make sure you are in a virtual environment. Don't mess up your machine's python version or packages. 

1. Install `ncclient`

```bash
pip install ncclient
```

2. Others

```bash
pip install python-dotenv loguru
```
