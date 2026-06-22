{ pkgs, ... }:
{
  cachix.enable = false;

  git-hooks.hooks = {
    treefmt.enable = true;
    nixfmt-rfc-style.enable = true;
  };
}
