#!/bin/bash
set -e

echo "=== NIMRS Agent Environment Setup ==="

# 1. PRE-CONFIG: Create the Nix config directory and disable sandboxing
echo "Configuring Nix..."
sudo mkdir -p /etc/nix
sudo bash -c 'cat <<EOF > /etc/nix/nix.conf
filter-syscalls = false
sandbox = false
experimental-features = nix-command flakes
EOF'

# 2. Download and run the installer
echo "Installing Nix..."
curl -L https://nixos.org/nix/install -o install-nix.sh
chmod +x install-nix.sh
./install-nix.sh --no-daemon --yes
rm install-nix.sh

# 3. Source the profile
. "$HOME/.nix-profile/etc/profile.d/nix.sh"

# 4. Install direnv and nix-direnv
echo "Installing direnv..."
nix-env -iA nixpkgs.direnv nixpkgs.nix-direnv

# 5. Configure Bash for direnv
echo "Configuring shell hooks..."
if ! grep -q "direnv hook bash" ~/.bashrc; then
  echo 'eval "$(direnv hook bash)"' >>~/.bashrc
fi
eval "$(direnv hook bash)"

# 6. Configure nix-direnv
mkdir -p ~/.config/direnv
echo "source $HOME/.nix-profile/share/nix-direnv/direnvrc" >>~/.config/direnv/direnvrc

# 7. Initialize current directory
echo "Initializing repository environment..."
direnv allow .

echo "=== Setup Complete! ==="
echo "You can now enter the environment by running: direnv allow"
