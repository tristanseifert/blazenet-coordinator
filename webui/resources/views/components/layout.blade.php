<!DOCTYPE html>
<html lang="{{ str_replace('_', '-', app()->getLocale()) }}">
    <head>
        <meta charset="utf-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>{{ $title ?? env('APP_NAME') }}</title>

        <link rel="stylesheet" href="{{ mix('/css/app.css') }}" />
@stack('stylesheets')
    </head>
    <body {{ $attributes->merge(['class' => 'app']) }}>
        <div id="page-wrapper">
            <x-navbar />

            <div class="container flash-container">
                <x-session-messages />
            </div>

            {{ $slot }}
        </div>

        {{-- footer --}}
        <x-layout-footer />

        {{-- this hidden form is used for logging out --}}
        {{--<form id="global-logout-form" name="logoutform" action="{{ route('logout') }}" method="POST">
            @csrf
        </form>--}}

        <script src="{{ mix('/js/manifest.js') }}"></script>
        <script src="{{ mix('/js/vendor.js') }}"></script>
        <script src="{{ mix('/js/app.js') }}"></script>
        @stack('scripts')
    </body>
</html>
