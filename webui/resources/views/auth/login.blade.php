<x-layout class="auth-login" showNavbar="0" showFlashes="0">
<x-slot name="title">{{ __('auth.login.title') }}</x-slot>

<section class="section">
    <div class="container is-max-desktop">
        <div class="content">
            <h1>{{ __('auth.login.heading') }}</h1>
            <p>{{ __('auth.login.description') }}</p>
        </div>

        <div class="flash-container">
            <x-session-messages />
        </div>

        <div class="box">
        @if ($errors->any())
            <x-alert type="warning" kind="message">
                <strong>{{ __('auth.login.failure') }}</strong>
                <ul>
                    @foreach ($errors->all() as $error)
                        <li>{{ $error }}</li>
                    @endforeach
                </ul>
            </x-alert>
        @endif

            <div class="content">
                <p>{{ __('auth.login.local.description') }}</p>
            </div>

            <form method="POST">
                <div class="field">
                    <label class="label">{{ __('auth.login.userlabel') }}</label>
                    <div class="control has-icons-left">
                        <input class="input @error('username') is-danger @enderror" type="text" name="username" value="{{ old('username') }}">
                        <span class="icon is-left">
                            <i class="fas fa-user"></i>
                        </span>
                    </div>
                    @error('username')
                        <p class="help is-danger">{{ $message }}</p>
                    @enderror
                </div>

                <div class="field">
                    <label class="label">{{ __('auth.login.passwordlabel') }}</label>
                    <div class="control has-icons-left">
                        <input class="input @error('password') is-danger @enderror" type="password" name="password">
                        <span class="icon is-left">
                            <i class="fas fa-lock"></i>
                        </span>
                    </div>
                    @error('password')
                        <p class="help is-danger">{{ $message }}</p>
                    @enderror
                </div>

                <div class="field">
                    <label class="checkbox">
                        <input type="checkbox" name="remember" value="1">
                        {{ __('auth.login.rememberlabel') }}
                    </label>
                    <p class="help">{{ __('auth.login.rememberhelp') }}</p>
                </div>

                <button class="button is-primary">{{ __('auth.login.button') }}</button>
                @csrf
            </form>
        </div>
    </div>
</section>

</x-layout>
