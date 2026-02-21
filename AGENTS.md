# 1. Install Nix in multi-user mode (silently)

curl -L https://nixos.org/nix/install | sh -s -- --daemon --yes

# 2. Source the nix profile so 'nix' commands are available

. /nix/var/nix/profiles/default/etc/profile.d/nix-sh

# 3. (Optional) Enable Flakes if you use them

mkdir -p ~/.config/nix
echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf

# 4. Use your existing nix-shell to install dependencies

# This ensures Jules uses the EXACT same versions as your homelab

nix-shell --run "echo 'Nix environment is ready for Jules!'"

# 5. Utilize the tools and environment setup in flake.nix

# 6. Note: use treefmt from the nix shell environment to format changes prior to commiting
