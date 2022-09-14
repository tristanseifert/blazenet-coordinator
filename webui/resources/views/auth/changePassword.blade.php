<x-layout class="auth-change-password" showNavbar="0" showFlashes="0">
<x-slot name="title">{{ __('auth.changePassword.title') }}</x-slot>

<section class="section">
    <div class="container is-max-desktop">
        <div class="content">
            <h1>{{ __('auth.changePassword.heading') }}</h1>
            <p>{{ __('auth.changePassword.description') }}</p>
        </div>

        <div class="flash-container">
            <x-session-messages />
        </div>

        <div class="box">
        @if ($errors->any())
            <x-alert type="warning" kind="message">
                <strong>{{ __('auth.changePassword.failure') }}</strong>
                <ul>
                    @foreach ($errors->all() as $error)
                        <li>{{ $error }}</li>
                    @endforeach
                </ul>
            </x-alert>
        @endif

            <div class="content">
                <p>{{ __('auth.changePassword.description2') }}</p>
            </div>

            <form method="POST">
                {{-- username (just for show) --}}
                <div class="field">
                    <label class="label">{{ __('auth.changePassword.userLabel') }}</label>
                    <div class="control has-icons-left">
                        <input class="input @error('username') is-danger @enderror" type="text" name="username" value="{{ auth()->user()->username }}" disabled>
                        <span class="icon is-left">
                            <i class="fas fa-user"></i>
                        </span>
                    </div>
                    @error('username')
                        <p class="help is-danger">{{ $message }}</p>
                    @enderror
                </div>

                {{-- current password --}}
                <div class="field">
                    <label class="label">{{ __('auth.changePassword.currentPasswordLabel') }}</label>
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

                <div class="field"><br /></div>

                {{-- new password (first field) --}}
                <div class="field">
                    <label class="label">{{ __('auth.changePassword.newPasswordLabel') }}</label>
                    <div class="control has-icons-left">
                        <input class="input @error('newPassword') is-danger @enderror" type="password" name="newPassword">
                        <span class="icon is-left">
                            <i class="fas fa-lock"></i>
                        </span>
                    </div>
                    @error('newPassword')
                        <p class="help is-danger">{{ $message }}</p>
                    @enderror
                </div>

                {{-- new password (confirmation) --}}
                <div class="field">
                    <label class="label">{{ __('auth.changePassword.newPassword2Label') }}</label>
                    <div class="control has-icons-left">
                        <input class="input @error('newPassword_comfirmation') is-danger @enderror" type="password" name="newPassword_confirmation">
                        <span class="icon is-left">
                            <i class="fas fa-lock"></i>
                        </span>
                    </div>
                    @error('newPassword_confirmation')
                        <p class="help is-danger">{{ $message }}</p>
                    @enderror
                </div>
                {{-- actions --}}
                @csrf
                <div class="buttons is-right">
                    <a class="logout button is-light">{{ __('auth.changePassword.cancel') }}</a>
                    <button class="button is-primary" type="submit">{{ __('auth.changePassword.submit') }}</button>
                </div>
            </form>
        </div>
    </div>
</section>

</x-layout>
