// ./tools/scripts/generate-release-notes.js

/**
 * Generates the release notes for a github release.
 *
 * Arguments:
 * 1 - new version tag
 * 2 - commit SHA of new release
 * 3 - optional prerelease tag prefix to stay within one release lane
 * 4 - optional target branch name for the note header
 */
const token = process.env.GITHUB_TOKEN;
const version = process.argv[2];
const comittish = process.argv[3];
const releaseTagPrefix = process.argv[4] || "";
const targetBranch = process.argv[5] || "";
const repo = process.env.REPOSITORY_NAME;
const owner = process.env.GITHUB_REPOSITORY_OWNER;

async function request_json( path, options = {} ) {
    const response = await fetch( `https://api.github.com${path}`, {
        ...options,
        headers: {
            "Accept": "application/vnd.github+json",
            "Authorization": `Bearer ${token}`,
            "User-Agent": "Cataclysm-AOL-release-notes",
            "X-GitHub-Api-Version": "2022-11-28",
            ...( options.headers || {} ),
        },
    } );
    const responseText = await response.text();
    let responseData = {};
    if( responseText ) {
        try {
            responseData = JSON.parse( responseText );
        } catch( error ) {
            throw new Error( `GitHub API returned non-JSON HTTP ${response.status}: ${responseText}` );
        }
    }
    if( !response.ok ) {
        throw new Error( `GitHub API returned HTTP ${response.status}: ${responseText}` );
    }
    return responseData;
}

async function main() {
    if( !token || !version || !comittish || !repo || !owner ) {
        throw new Error( "GITHUB_TOKEN, tag, commit, repository, and owner are required" );
    }
    let previousTag = "";
    let latestStableTag = "";
    let latestAnyTag = "";

    const releasesResponse = await request_json(
        `/repos/${encodeURIComponent( owner )}/${encodeURIComponent( repo )}/releases?per_page=100`
    );

    if( Array.isArray( releasesResponse ) ) {
        for( const responseData of releasesResponse ) {
            if( responseData.draft ) {
                continue;
            }

            if( !latestAnyTag ) {
                latestAnyTag = responseData.tag_name;
            }
            if( !responseData.prerelease && !latestStableTag ) {
                latestStableTag = responseData.tag_name;
            }

            if( responseData.prerelease && releaseTagPrefix && responseData.tag_name.startsWith( releaseTagPrefix ) ) {
                previousTag = responseData.tag_name;
                break;
            }
        }
    }

    if( !previousTag ) {
        previousTag = latestStableTag || latestAnyTag;
    }

    const requestBody = {
        tag_name: version,
        target_commitish: comittish,
    };
    if( previousTag ) {
        requestBody.previous_tag_name = previousTag;
    }

    const response = await request_json(
        `/repos/${encodeURIComponent( owner )}/${encodeURIComponent( repo )}/releases/generate-notes`,
        {
            method: "POST",
            body: JSON.stringify( requestBody ),
            headers: { "Content-Type": "application/json" },
        }
    );

    const noteSections = response.body?.split( '\n\n' ) ?? [];
    const trimmedSections = [];
    const githubNotesMaxCharLength = 125000;
    const maxSectionLength = noteSections.length > 0 ? githubNotesMaxCharLength / noteSections.length : githubNotesMaxCharLength;

    for( let i = 0; i < noteSections.length; i++ ) {
        if( noteSections[i].length > githubNotesMaxCharLength ) {
            const lastLineIndex =
                noteSections[i].substring( 0, maxSectionLength ).split( '\n' ).length - 1;
            const trimmed =
                noteSections[i]
                .split( '\n' )
                .slice( 0, lastLineIndex - 1 )
                .join( '\n' ) +
                `\n... (+${noteSections[i].split( '\n' ).length - ( lastLineIndex + 1 )} others)`;
            trimmedSections.push( trimmed );
            continue;
        }

        trimmedSections.push( noteSections[i] );
    }

    const headerLines = [];
    if( targetBranch ) {
        headerLines.push( `Target branch: \`${targetBranch}\`` );
    }
    if( previousTag ) {
        headerLines.push( `Previous release reference: \`${previousTag}\`` );
    }

    if( headerLines.length > 0 ) {
        console.log( `${headerLines.join( '\n' )}\n\n${trimmedSections.join( '\n\n' )}` );
    } else {
        console.log( trimmedSections.join( '\n\n' ) );
    }
}

main().catch( ( e ) => {
    console.error( `Failed generating release notes with error: ${e}` );
    process.exit( 1 );
} );
