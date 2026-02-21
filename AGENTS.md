# 1. PRE-CONFIG: Create the Nix config directory and disable sandboxing

# This prevents the seccomp error during the installation's self-test.

sudo mkdir -p /etc/nix
sudo bash -c 'cat <<EOF > /etc/nix/nix.conf
filter-syscalls = false
sandbox = false
experimental-features = nix-command flakes
EOF'

# 2. Download and run the installer

curl -L https://nixos.org/nix/install -o install-nix.sh
chmod +x install-nix.sh
./install-nix.sh --no-daemon --yes

# 3. Source the profile

. "$HOME/.nix-profile/etc/profile.d/nix.sh"

# 4. Install direnv and nix-direnv

nix-env -iA nixpkgs.direnv nixpkgs.nix-direnv

# 5. Configure Bash for direnv

echo 'eval "$(direnv hook bash)"' >> ~/.bashrc
eval "$(direnv hook bash)"

# 6. Configure nix-direnv

mkdir -p ~/.config/direnv
echo "source $HOME/.nix-profile/share/nix-direnv/direnvrc" >> ~/.config/direnv/direnvrc

# 7. Enter the app directory and allow direnv

cd /app
direnv allow .
eval "$(direnv export bash)"

echo "Nix environment successfully forced into existence."

# 5. Utilize the tools and environment setup in flake.nix

# 6. The following can be used to validate we are ready for CI to build this.

# nix develop --command ci-ready
