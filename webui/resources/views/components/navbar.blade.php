{{-- Top nav bar --}}
<nav class="navbar is-fixed-top main-navbar" role="navigation" aria-label="main navigation">
    <div class="navbar-brand">
        <a class="navbar-item" href="{{ route('homepage') }}">{{ config('app.name') }}</a>

        <a role="button" class="navbar-burger" aria-label="menu" aria-expanded="false" data-target="navbarBasicExample">
        <span aria-hidden="true"></span>
        <span aria-hidden="true"></span>
        <span aria-hidden="true"></span>
        </a>
    </div>

    <div class="navbar-menu">
        <div class="navbar-start">
            <a class="navbar-item" href="{{ route('homepage') }}">{{ __('navbar.link.home') }}</a>
        </div>

        {{-- actions on the right side (user menu) --}}
        <div class="navbar-end">
            {{-- unauthenticated users get log in buttons --}}
@guest
            <div class="navbar-item authenticate-guest">
                <div class="buttons">
                    <a class="button is-light" href="{{ route('login') }}">
                        {{ __('navbar.auth.login') }}
                    </a>
                </div>
            </div>
@endguest

            {{-- authenticated users show the user menu --}}
@auth
            <div class="navbar-item has-dropdown is-hoverable">
                <a class="navbar-link">
                    {{ __('navbar.auth.usermenu', ['name' => Auth::user()->username]) }}
                </a>

                <div class="navbar-dropdown is-right">
                    <hr class="navbar-divider">
                    <a class="navbar-item logout" href="#">
                        <span class="icon-text">
                            <span class="icon">
                                <i class="fas fa-door-open"></i>
                            </span>
                            <span>
                                 {{ __('navbar.auth.usermenu.logout') }}
                            </span>
                        </span>
                    </a>
                </div>
            </div>
@endauth
        </div>
    </div>
</nav>
